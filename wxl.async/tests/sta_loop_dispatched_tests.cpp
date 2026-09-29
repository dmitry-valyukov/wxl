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

/// What an operation that opens something hands back: a resource of its own, released
/// by whoever ends up holding it.
class handle_like
{
public:
    handle_like(std::atomic<int>& released, std::thread::id& released_on)
        : released_(&released), released_on_(&released_on) {}

    handle_like(handle_like&& other) noexcept
        : released_(std::exchange(other.released_, nullptr)), released_on_(other.released_on_) {}

    handle_like& operator=(handle_like&&) = delete;

    ~handle_like() {
        if (!released_) return;

        *released_on_ = std::this_thread::get_id();
        ++*released_;
    }

private:
    std::atomic<int>* released_;
    std::thread::id* released_on_;
};

/// Starts an opening and gives it up once the pool is inside it.
task opens_and_changes_its_mind(hevent& started, hevent& gate, std::atomic<int>& released,
                                std::thread::id& released_on) {
    auto opening = sta_loop::async_call(orphanable, [&started, &gate, &released, &released_on] {
        started.set();
        gate.wait();
        return handle_like(released, released_on);
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

TEST(StaLoopDispatchedTest, AnOrphanGivenUpIsNotWaitedForAndReleasesWhatItBringsHere) {
    hevent started{true};
    hevent gate{true};
    std::atomic<int> released{0};
    std::thread::id released_on;

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

    task work = opens_and_changes_its_mind(started, gate, released, released_on);

    unwound.release();
    watchdog.join();

    EXPECT_FALSE(had_to_wait.load()) << "the frame waited for an operation that touches nothing of it";
    EXPECT_TRUE(work.done());
    EXPECT_THROW(work.result(), std::runtime_error);
    EXPECT_EQ(released.load(), 0);

    // It runs to its end on the pool, comes back through the queue, and what it
    // brought is released as it is deleted -- on this thread.
    gate.set();

    wait_until([&] { return released.load() != 0; });

    EXPECT_EQ(released.load(), 1);
    EXPECT_EQ(released_on, std::this_thread::get_id());
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
