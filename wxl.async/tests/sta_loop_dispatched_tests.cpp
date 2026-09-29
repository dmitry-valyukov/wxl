// The loop on a dispatcher queue: where an orphanable operation is carried out, how it
// comes back, and what happens to one that was given up on the way.
#include "dispatched_environment.h"

using namespace wxl::core;
using namespace wxl::async;

namespace {

task asks_where(std::thread::id& worker, std::thread::id& orphan) {
    worker = co_await sta_loop::async_call([] { return std::this_thread::get_id(); });
    orphan = co_await sta_loop::async_call(orphanable, [] { return std::this_thread::get_id(); });
}

/// Where a resource was let go of, and the moment it was.
struct release_record {
    std::atomic<int> times{0};
    std::thread::id on;

    /// Manual-reset, set as the resource goes.
    hevent happened{true};
};

/// What an operation that opens something hands back: a resource of its own.
class handle_like
{
public:
    explicit handle_like(release_record& record) : record_(&record) {}

    handle_like(handle_like&& other) noexcept : record_(std::exchange(other.record_, nullptr)) {}

    handle_like& operator=(handle_like&&) = delete;

    ~handle_like() {
        if (!record_) return;

        record_->on = std::this_thread::get_id();
        ++record_->times;
        record_->happened.set();
    }

private:
    release_record* record_;
};

/// Says, as it goes, that the operation carrying it has been deleted: it rides in the
/// capture of the body.
class deletion_mark
{
public:
    explicit deletion_mark(std::atomic<bool>& deleted) : deleted_(&deleted) {}

    deletion_mark(deletion_mark&& other) noexcept
        : deleted_(std::exchange(other.deleted_, nullptr)) {}

    deletion_mark& operator=(deletion_mark&&) = delete;

    ~deletion_mark() {
        if (deleted_) *deleted_ = true;
    }

private:
    std::atomic<bool>* deleted_;
};

/// Starts an opening and gives it up once the pool is inside it.
task opens_and_changes_its_mind(hevent& started, hevent& gate, release_record& record,
                                std::atomic<bool>& deleted) {
    auto opening =
        sta_loop::async_call(orphanable, [&started, &gate, &record, mark = deletion_mark(deleted)] {
            started.set();
            gate.wait();
            return handle_like(record);
        });

    started.wait();

    throw std::runtime_error("changed its mind");

    co_await opening;
}

}  // namespace

TEST(StaLoopDispatchedTest, AnOrphanableOperationRunsOnThePoolAndNotOnTheWorker) {
    std::thread::id worker;
    std::thread::id orphan;

    task work = asks_where(worker, orphan);

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_NE(worker, std::thread::id{});
    EXPECT_NE(orphan, std::thread::id{});
    EXPECT_NE(worker, std::this_thread::get_id());
    EXPECT_NE(orphan, std::this_thread::get_id());
    EXPECT_NE(orphan, worker);
}

TEST(StaLoopDispatchedTest, AnOrphanGivenUpWhileItRunsLetsGoOfWhatItMakesAsSoonAsItIsMade) {
    hevent started{true};
    hevent gate{true};
    release_record record;
    std::atomic<bool> deleted{false};

    // Only a scheme that waits for an orphan would keep the call below from returning
    // while the gate is shut. The watchdog opens it after a while, so that such a
    // scheme fails the test instead of hanging the binary.
    std::binary_semaphore unwound{0};
    std::atomic<bool> had_to_wait{false};
    std::jthread watchdog([&] {
        if (unwound.try_acquire_for(std::chrono::seconds(10))) return;

        had_to_wait = true;
        gate.set();
    });

    task work = opens_and_changes_its_mind(started, gate, record, deleted);

    unwound.release();
    watchdog.join();

    EXPECT_FALSE(had_to_wait.load()) << "the frame waited for an operation that touches nothing of it";
    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(record.times.load(), 0);

    // It runs to its end on the pool, and what it made goes there and then: the
    // message loop has not turned, and nobody is left to take it.
    gate.set();
    record.happened.wait();

    EXPECT_EQ(record.times.load(), 1);
    EXPECT_NE(record.on, std::this_thread::get_id());

    // The operation itself still comes back through the queue, to be deleted here.
    wait_until([&] { return deleted.load(); });
}

TEST(StaLoopDispatchedTest, AnOrphanGivenUpAfterItsBodyLetsGoOfWhatItMadeWithoutTheLoopTurning) {
    // Given up once the body has made the resource. Whether the pool has got as far as
    // saying so or not, the resource goes -- by the hand of this thread or of the pool
    // -- while this thread stands still.
    release_record record;
    hevent made{true};
    std::atomic<bool> deleted{false};

    {
        auto opening =
            sta_loop::async_call(orphanable, [&record, &made, mark = deletion_mark(deleted)] {
                handle_like resource(record);

                made.set();
                return resource;
            });

        made.wait();
    }

    record.happened.wait();

    EXPECT_EQ(record.times.load(), 1);

    wait_until([&] { return deleted.load(); });
}

TEST(StaLoopDispatchedTest, AnOrphanComingBackIsTakenByTheCoAwaitThatFollows) {
    // Back before anybody awaits it: the queue delivers it with no coroutine to resume,
    // and the co_await that comes later finds it there.
    int value = 0;

    auto answers = [](int& out) -> task {
        auto answer = sta_loop::async_call(orphanable, [] { return 7; });

        co_await sta_loop::async_call([] {});
        co_await sta_loop::async_call(orphanable, [] {});

        out = co_await answer;
    };

    task work = answers(value);

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(value, 7);
}

TEST(StaLoopDispatchedTest, AnExceptionFromThePoolArrivesAtTheCoAwait) {
    std::string message;

    auto fails = [](std::string& out) -> task {
        try {
            co_await sta_loop::async_call(orphanable,
                                          [] { throw std::runtime_error("from the pool"); });
            out = "no exception";
        } catch (const std::exception& e) {
            out = e.what();
        }
    };

    task work = fails(message);

    wait_until([&] { return work.done(); });
    work.result();

    EXPECT_EQ(message, "from the pool");
}
