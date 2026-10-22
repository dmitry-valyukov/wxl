// A task lives in a named place -- a local, or a field of whoever knows when to stop it -- and
// there is no fire-and-forget: what the body holds goes when the body ends, whether the task
// is still kept or not; dropping a task whose body runs stops the body, and its locals go
// there and then; an object that keeps its own coroutine in a field stops it by going; and
// what left the body is read from the task, by result() or by a co_await, or not at all --
// a failure nobody reads is nothing more than that.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

namespace {

// Something whose destruction can be seen from outside: what a coroutine holds has to be let
// go of, whichever way the coroutine ends.
struct Trace {
    bool* released;

    explicit Trace(bool* flag) noexcept : released(flag) {}
    Trace(const Trace&) = delete;
    ~Trace() { *released = true; }
};

/// Parks the coroutine where the test can resume it, and ends the wait with what the test put
/// in -- nothing, or an exception: the shape of every real awaitable in wxl, whose await_resume()
/// can throw, since a result, an error and a cancellation all arrive through it.
struct ThrowingCapture {
    std::coroutine_handle<>* out;
    std::exception_ptr error;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const {
        if (error) std::rethrow_exception(error);
    }
};

/// The same wait with an await_resume() that cannot throw. MSVC x64 under /O2 with a
/// synchronous exception model has skipped the locals' destructors of a coroutine whose body
/// cannot throw; the two shapes side by side say whether a coroutine is covered either way.
struct Capture {
    std::coroutine_handle<>* out;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> waiter) const noexcept { *out = waiter; }
    void await_resume() const noexcept {}
};

task<> runs_through(int& sink) {
    sink = 1;
    co_return;
}

task<> waits_then_finishes(std::coroutine_handle<>& out, bool* released) {
    const Trace trace{released};

    co_await Capture{&out};
}

task<> waits_then_finishes_throwing(std::coroutine_handle<>& out, bool* released,
                                    std::exception_ptr error = {}) {
    const Trace trace{released};

    co_await ThrowingCapture{&out, error};
}

task<> cancelled_at_once() {
    throw operation_canceled_exception{};
    co_return;
}

task<> fails_at_once(std::string_view what) {
    throw std::runtime_error(std::string(what));
    co_return;
}

/// What result() threw, or nothing.
std::string failure_of(task<>& t) {
    try {
        t.result();
    } catch (const operation_canceled_exception&) {
        return "cancelled";
    } catch (const std::runtime_error& failure) {
        return failure.what();
    }

    return {};
}

}  // namespace

// The body runs where it is called, as far as its first suspension.
TEST(TaskOwnershipTest, RunsWhereItIsCalled) {
    int sink = 0;
    task<> t = runs_through(sink);

    EXPECT_EQ(sink, 1);
    EXPECT_TRUE(t.done());
}

// What the body held goes when the body ends, with the task still kept: the task holds the end
// and nothing of the frame.
TEST(TaskOwnershipTest, WhatTheBodyHeldGoesAsTheBodyEndsWithTheTaskStillKept) {
    for (const bool throwing : {false, true}) {
        SCOPED_TRACE(throwing ? "through an awaiter that can throw" : "through one that cannot");

        std::coroutine_handle<> waiter;
        bool released = false;
        task<> kept = throwing ? waits_then_finishes_throwing(waiter, &released)
                               : waits_then_finishes(waiter, &released);

        ASSERT_TRUE(waiter);
        EXPECT_FALSE(released);

        waiter.resume();

        EXPECT_TRUE(released);
        EXPECT_TRUE(kept.done());
        EXPECT_NO_THROW(kept.result());
    }
}

// Dropping the task while the body waits stops the body: its locals go there and then, and
// nothing resumes it.
TEST(TaskOwnershipTest, DroppingATaskStopsItsBodyAndLetsGoOfWhatItHeld) {
    for (const bool throwing : {false, true}) {
        SCOPED_TRACE(throwing ? "through an awaiter that can throw" : "through one that cannot");

        std::coroutine_handle<> waiter;
        bool released = false;
        task<> kept = throwing ? waits_then_finishes_throwing(waiter, &released)
                               : waits_then_finishes(waiter, &released);
        ASSERT_TRUE(waiter);

        kept = {};

        EXPECT_TRUE(released);
    }
}

// What left the body -- a cancellation, a failure -- waits in the task, for result() as often as
// it is asked.
TEST(TaskOwnershipTest, WhatLeftTheBodyIsReadFromTheTask) {
    task<> cancelled = cancelled_at_once();
    task<> failed = fails_at_once("no such file");

    EXPECT_EQ(failure_of(cancelled), "cancelled");
    EXPECT_EQ(failure_of(failed), "no such file");
    EXPECT_EQ(failure_of(failed), "no such file");

    std::coroutine_handle<> waiter;
    bool released = false;
    task<> later = waits_then_finishes_throwing(waiter, &released,
                                                std::make_exception_ptr(std::runtime_error("after the wait")));
    waiter.resume();

    EXPECT_TRUE(released);
    EXPECT_EQ(failure_of(later), "after the wait");
}

// A failure nobody reads is nothing: the task goes with it unread, and the process goes on.
TEST(TaskOwnershipTest, AFailureNobodyReadsIsNothing) {
    {
        task<> failed = fails_at_once("never read");
        EXPECT_TRUE(failed.done());
    }

    SUCCEED() << "the process is still here";
}

// The count of live frames: up as a frame is made, down as it is unmade -- by the body ending,
// by the task being dropped while it runs, by a failure or a cancellation on the way. Only a
// build that checks counts.
TEST(TaskOwnershipTest, ALiveFrameIsCountedUntilItIsUnmade) {
    if constexpr (detail::checked) {
        const std::size_t before = live_frames();
        std::coroutine_handle<> waiter;
        bool released = false;

        task<> ends = waits_then_finishes(waiter, &released);
        EXPECT_EQ(live_frames(), before + 1);

        waiter.resume();
        EXPECT_EQ(live_frames(), before);

        task<> dropped = waits_then_finishes(waiter, &released);
        EXPECT_EQ(live_frames(), before + 1);

        dropped = {};
        EXPECT_EQ(live_frames(), before);

        int sink = 0;
        task<> at_once = runs_through(sink);
        task<> cancelled = cancelled_at_once();
        task<> failed = fails_at_once("counted out");
        EXPECT_EQ(live_frames(), before);
    } else {
        GTEST_SKIP() << "frames are counted only in a build that checks coroutines";
    }
}

// A frame comes from the STA pool, and the pool asserts the thread in debug builds: a task's
// coroutine started anywhere else is a broken rule, reported to stderr rather than to a dialog
// that would hang the run.
void start_a_task_on_another_thread() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    int sink = 0;
    std::thread([&sink] { task<> t = runs_through(sink); }).join();
}

namespace {

/// What keeps the task of a body that replaces it.
struct holds_its_task {
    task<> held;
};

/// Once resumed, the body assigns a new task over the field that holds it -- while it runs.
task<> replaces_its_own_task(holds_its_task& holder, std::coroutine_handle<>& out) {
    co_await Capture{&out};

    int sink = 0;
    holder.held = runs_through(sink);
}

void replace_a_running_task() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    holds_its_task holder;
    std::coroutine_handle<> out;
    holder.held = replaces_its_own_task(holder, out);
    out.resume();
}

}  // namespace

// A body may not replace the task that holds it while it runs: that would destroy the frame it
// runs in. A build that checks stops at it.
TEST(TaskOwnershipTest, ABodyDoesNotReplaceTheTaskThatHoldsIt) {
    if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    EXPECT_DEATH(replace_a_running_task(), "destroyed while its coroutine runs");
}

TEST(TaskOwnershipTest, AFrameMadeOffTheStaThreadIsABrokenRule) {
#ifdef NDEBUG
    GTEST_SKIP() << "the pool asserts its thread in debug builds only";
#else
    EXPECT_DEATH(start_a_task_on_another_thread(), "thread_guard: thread mismatch");
#endif
}

namespace {

// What an object that keeps its own coroutine saw of its life, kept outside it so that it can
// still be read once the object is gone.
struct Notes {
    int deaths = 0;
    int resumed = 0;
};

/// An object that keeps the coroutine it runs in a field: the object knows when to stop it --
/// when it goes, at the latest -- and the coroutine may use `this` throughout, since it cannot
/// outlive the field. The field is the last member: it goes first, before anything its body
/// touches.
class Watches
{
public:
    explicit Watches(Notes& notes) noexcept : notes_(notes) {}

    /// Starts the wait in the constructor, before anybody else holds the object.
    Watches(Notes& notes, std::coroutine_handle<>& out) : notes_(notes) { start(out); }

    ~Watches() { ++notes_.deaths; }

    void start(std::coroutine_handle<>& out, std::exception_ptr error = {}) {
        watching_ = waits(out, std::move(error));
    }

    void stop() { watching_ = {}; }

    task<>& watching() noexcept { return watching_; }

private:
    task<> waits(std::coroutine_handle<>& out, std::exception_ptr error) {
        co_await ThrowingCapture{&out, std::move(error)};
        ++notes_.resumed;
    }

    Notes& notes_;
    task<> watching_;
};

}  // namespace

// The object goes while its coroutine waits: the field goes first and stops the body, which is
// never resumed and never touches the object again.
TEST(TaskOwnershipTest, AnObjectThatGoesStopsTheCoroutineItKeeps) {
    Notes notes;
    std::coroutine_handle<> waiter;
    [[maybe_unused]] std::size_t before = 0;
    if constexpr (detail::checked) before = live_frames();

    {
        Watches object(notes);
        object.start(waiter);
        ASSERT_TRUE(waiter);
        if constexpr (detail::checked) {
            EXPECT_EQ(live_frames(), before + 1);
        }
    }

    EXPECT_EQ(notes.deaths, 1);
    EXPECT_EQ(notes.resumed, 0);
    if constexpr (detail::checked) {
        EXPECT_EQ(live_frames(), before);
    }
}

// Started in the constructor, the coroutine is the object's from its birth: the object stops it
// by going, and lets it end by staying.
TEST(TaskOwnershipTest, ACoroutineStartedInTheConstructorIsTheObjectsToo) {
    Notes notes;
    std::coroutine_handle<> waiter;

    {
        auto object = std::make_unique<Watches>(notes, waiter);
        ASSERT_TRUE(waiter);

        waiter.resume();
        EXPECT_EQ(notes.resumed, 1);
        EXPECT_TRUE(object->watching().done());
    }
    EXPECT_EQ(notes.deaths, 1);

    {
        auto object = std::make_unique<Watches>(notes, waiter);
    }
    EXPECT_EQ(notes.deaths, 2);
    EXPECT_EQ(notes.resumed, 1) << "the coroutine of an object gone was resumed";
}

// The object stops its coroutine itself, and starts another: the field is assigned over.
TEST(TaskOwnershipTest, AnObjectStopsItsCoroutineByDroppingItsField) {
    Notes notes;
    std::coroutine_handle<> first, second;
    Watches object(notes);

    object.start(first);
    object.stop();
    EXPECT_EQ(notes.resumed, 0);

    object.start(first);
    object.start(second);
    EXPECT_EQ(notes.resumed, 0) << "the coroutine assigned over was resumed";

    second.resume();
    EXPECT_EQ(notes.resumed, 1);
}

// However the body ends, what left it is the field's to read: a failure after the wait...
TEST(TaskOwnershipTest, AFailureOfTheCoroutineAnObjectKeepsIsReadFromItsField) {
    Notes notes;
    std::coroutine_handle<> waiter;
    Watches object(notes);

    object.start(waiter, std::make_exception_ptr(std::runtime_error("after the wait")));
    waiter.resume();

    EXPECT_EQ(notes.resumed, 0);
    EXPECT_EQ(failure_of(object.watching()), "after the wait");

    // ...and a cancellation, the ordinary end of a wait that will not come.
    object.start(waiter, std::make_exception_ptr(operation_canceled_exception{}));
    waiter.resume();

    EXPECT_EQ(failure_of(object.watching()), "cancelled");
}

namespace {

// A task awaiting a task: a scenario awaiting the step it hands its work to. The step stands on
// ThrowingCapture, so it ends with whatever the test resumes it with -- a value, a failure or a
// cancellation -- and that travels up through the co_await into the scenario, and from there to
// whoever reads the scenario.

task<int> answers_when_resumed(std::coroutine_handle<>& out, std::exception_ptr error) {
    co_await ThrowingCapture{&out, error};
    co_return 42;
}

task<int> passes_the_answer_on(std::coroutine_handle<>& out, std::exception_ptr error) {
    co_return 1 + co_await answers_when_resumed(out, error);
}

task<> awaits_the_answer(std::coroutine_handle<>& out, std::exception_ptr error, int& got, bool* released) {
    const Trace trace{released};

    got = co_await answers_when_resumed(out, error);
}

task<> awaits_the_answer_through_a_chain(std::coroutine_handle<>& out, std::exception_ptr error, int& got,
                                         bool* released) {
    const Trace trace{released};

    got = co_await passes_the_answer_on(out, error);
}

}  // namespace

TEST(TaskOwnershipTest, TheValueOfATaskReachesTheTaskAwaitingIt) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    task<> scenario = awaits_the_answer(out, {}, got, &released);

    ASSERT_TRUE(out);
    EXPECT_FALSE(released);

    out.resume();

    EXPECT_EQ(got, 42);
    EXPECT_TRUE(released);
    EXPECT_EQ(failure_of(scenario), "");
}

TEST(TaskOwnershipTest, AFailureOfTheTaskItAwaitsIsReadFromTheScenario) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    task<> scenario = awaits_the_answer(out, std::make_exception_ptr(std::runtime_error("the task failed")),
                                        got, &released);

    ASSERT_TRUE(out);
    out.resume();

    EXPECT_EQ(got, 0);
    EXPECT_TRUE(released);
    EXPECT_EQ(failure_of(scenario), "the task failed");
}

// The way a cancellation ends a chain: the wait at the bottom ends with it, every task above
// passes it on, and the scenario at the top ends with it for its owner to read.
TEST(TaskOwnershipTest, ACancellationClimbsAChainOfTasksToTheScenario) {
    std::coroutine_handle<> out;
    int got = 0;
    bool released = false;

    task<> scenario = awaits_the_answer_through_a_chain(
        out, std::make_exception_ptr(operation_canceled_exception{}), got, &released);

    ASSERT_TRUE(out);
    out.resume();

    EXPECT_EQ(got, 0);
    EXPECT_TRUE(released);
    EXPECT_EQ(failure_of(scenario), "cancelled");
}
