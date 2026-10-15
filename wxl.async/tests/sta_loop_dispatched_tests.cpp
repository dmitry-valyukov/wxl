// The loop on a dispatcher queue: where an orphanable operation is carried out, how it
// comes back, and what happens to one that was given up on the way.
#include "loop_environment.h"

#include <crtdbg.h>

using namespace wxl::core;
using namespace wxl::async;

namespace {

task<> asks_where(std::thread::id& worker, std::thread::id& orphan) {
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
task<> opens_and_changes_its_mind(hevent& started, hevent& gate, release_record& record,
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

    task<> work = asks_where(worker, orphan);

    sta_loop::run_until(work);

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

    task<> work = opens_and_changes_its_mind(started, gate, record, deleted);

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

    auto answers = [](int& out) -> task<> {
        auto answer = sta_loop::async_call(orphanable, [] { return 7; });

        co_await sta_loop::async_call([] {});
        co_await sta_loop::async_call(orphanable, [] {});

        out = co_await answer;
    };

    task<> work = answers(value);

    sta_loop::run_until(work);

    EXPECT_EQ(value, 7);
}

TEST(StaLoopDispatchedTest, AnExceptionFromThePoolArrivesAtTheCoAwait) {
    std::string message;

    auto fails = [](std::string& out) -> task<> {
        try {
            co_await sta_loop::async_call(orphanable,
                                          [] { throw std::runtime_error("from the pool"); });
            out = "no exception";
        } catch (const std::exception& e) {
            out = e.what();
        }
    };

    task<> work = fails(message);

    sta_loop::run_until(work);

    EXPECT_EQ(message, "from the pool");
}

// run_until() under the queue turns the thread's message loop: an answer from the pool
// reaches the detached frame waiting for it there, and the wait ends with that frame.
detached_task awaits_the_pool(bool& answered) {
    answered = co_await sta_loop::async_call(orphanable, [] {
        ::Sleep(50);
        return true;
    });
}

TEST(StaLoopDispatchedTest, RunUntilTurnsTheMessageLoopUntilTheConditionHolds) {
    const std::size_t before = detached_task::live_frames();
    bool answered = false;

    awaits_the_pool(answered);
    EXPECT_EQ(detached_task::live_frames(), before + 1);

    sta_loop::run_until([before] { return detached_task::live_frames() == before; });

    EXPECT_TRUE(answered);
}

// run_until(task) under the queue: the task's answer comes from the pool through the
// message loop, and the value comes back from the call.
task<int> asks_the_pool(int value) {
    co_return co_await sta_loop::async_call(orphanable, [value] {
        ::Sleep(50);
        return value;
    });
}

TEST(StaLoopDispatchedTest, RunUntilGivesTheValueATaskAnsweredFromThePoolEndsWith) {
    task<int> work = asks_the_pool(7);

    EXPECT_EQ(sta_loop::run_until(work), 7);
}

// An orphan whose task went at once is left to finish alone on the pool, and is counted while
// it does: run_until() on outstanding() -- what the end of a process waits for -- ends once
// it has come back through the queue and been deleted.
TEST(StaLoopDispatchedTest, AnOrphanLeftToFinishAloneIsWaitedForByRunUntil) {
    std::atomic<bool> deleted{false};

    {
        auto let_go = sta_loop::async_call(orphanable, [mark = deletion_mark(deleted)] {
            ::Sleep(50);
            return 0;
        });
    }

    EXPECT_NE(sta_loop::outstanding(), 0u) << "an orphan left to finish alone is not counted";
    EXPECT_FALSE(deleted.load());

    sta_loop::run_until([] { return sta_loop::outstanding() == 0; });

    EXPECT_TRUE(deleted.load()) << "the wait ended before the orphan was back";
}

// A broken rule is reported to stderr in every build, not to a dialog that would hang the
// run.
void report_broken_rules_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

// run_until() under the queue turns the thread's message loop, the thread's only one while it
// turns, in a test as at the end of a process. A WM_QUIT reaching it is somebody asking the
// thread to leave before what it waits for is done -- a broken rule, which stops the process
// rather than being carried past the wait. The operation takes long enough for the wait to be
// a real one; the quit is in the queue before it begins.
void quit_while_the_loop_waits() {
    report_broken_rules_to_stderr();

    task<int> pending = sta_loop::async_call(orphanable, [] {
        ::Sleep(100);
        return 0;
    });

    ::PostQuitMessage(0);
    sta_loop::run_until(pending);
}

TEST(StaLoopDispatchedDeathTest, AQuitPostedWhileTheLoopWaitsIsABrokenRule) {
    EXPECT_DEATH(quit_while_the_loop_waits(), "WM_QUIT");
}

// stop() checks that nothing is out rather than waiting for it: the loop is stopped once its
// work is over, and an operation still out then is one nobody is going to see back. Here its
// task is still there and nobody has waited -- the orphan is asleep on the pool as stop() is
// called.
void stop_with_an_operation_out() {
    report_broken_rules_to_stderr();

    task<int> pending = sta_loop::async_call(orphanable, [] {
        ::Sleep(100);
        return 0;
    });

    sta_loop::stop();
}

TEST(StaLoopDispatchedDeathTest, AnOperationOutAtStopIsABrokenRule) {
    EXPECT_DEATH(stop_with_an_operation_out(), "still out");
}
