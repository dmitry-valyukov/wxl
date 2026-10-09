#include <crtdbg.h>

#include <gtest/gtest.h>

#include "sta_pool.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

task<> returns_at_once(int& sink) {
    sink = 1;
    co_return;
}

task<> suspends_once(int& sink) {
    sink = 1;
    co_await std::suspend_always{};
    sink = 2;
}

task<> throws(std::string_view what) {
    throw std::runtime_error(std::string(what));
    co_return;
}

/// The control for the probe below: the same shape of coroutine, with its frame
/// from the ordinary allocator. Without it a passing test would prove only that
/// the probe sees nothing anywhere.
struct heap_task {
    struct promise_type;
    using handle_t = std::coroutine_handle<promise_type>;

    struct promise_type {
        heap_task get_return_object() { return heap_task{handle_t::from_promise(*this)}; }
        std::suspend_never initial_suspend() const noexcept { return {}; }
        std::suspend_always final_suspend() const noexcept { return {}; }
        void return_void() const noexcept {}
        void unhandled_exception() const noexcept {}
    };

    ~heap_task() {
        if (handle) handle.destroy();
    }

    handle_t handle;
};

heap_task heap_suspends_once(int& sink) {
    sink = 1;
    co_await std::suspend_always{};
    sink = 2;
}

/// Lets a test resume a coroutine by hand: the awaiter parks the handle where
/// the test can reach it, the way an operation of this module parks it in the
/// return channel.
struct parked {
    std::coroutine_handle<>& slot;

    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) noexcept { slot = here; }
    void await_resume() const noexcept {}
};

/// Counts its own destruction, to see in what order frames go.
struct notes_death {
    int& deaths;
    ~notes_death() { ++deaths; }
};

task<int> answers_after_a_pause(std::coroutine_handle<>& slot, int& deaths) {
    notes_death guard{deaths};
    co_await parked{slot};
    co_return 42;
}

task<int> answers_at_once() {
    co_return 7;
}

/// Throws, and to the compiler "gives" a T: the body of a coroutine with a value
/// has to end in co_return, and MSVC does not count a throw as its end.
template <class T>
[[noreturn]] T fail_with(const char* what) {
    throw std::runtime_error(what);
}

task<std::string> fails_after_a_pause(std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return fail_with<std::string>("inner");
}

task<std::unique_ptr<int>> hands_over_ownership() {
    co_return std::make_unique<int>(5);
}

task<> awaits_the_pause(std::coroutine_handle<>& slot, int& got, int& deaths) {
    notes_death guard{deaths};
    got = co_await answers_after_a_pause(slot, deaths);
}

task<> awaits_at_once(int& got) {
    got = co_await answers_at_once();
}

task<> catches_from_the_inner(std::coroutine_handle<>& slot, std::string& what) {
    try {
        co_await fails_after_a_pause(slot);
    } catch (const std::runtime_error& failure) {
        what = failure.what();
    }
}

task<int> adds_one(std::coroutine_handle<>& slot, int& deaths) {
    co_return 1 + co_await answers_after_a_pause(slot, deaths);
}

task<> awaits_a_chain(std::coroutine_handle<>& slot, int& got, int& deaths) {
    got = co_await adds_one(slot, deaths);
}

/// Joins a task somebody else keeps: co_await of an lvalue.
task<> joins(task<int>& kept, int& got, int& deaths) {
    notes_death guard{deaths};
    got = co_await kept;
}

/// Awaits an ended task<> twice: what left it is thrown both times.
task<> awaits_twice(task<>& failed, int& caught) {
    for (int i = 0; i < 2; ++i) {
        try {
            co_await failed;
        } catch (const std::runtime_error&) {
            ++caught;
        }
    }
}

task<int> owes_a_value_and_fails() {
    co_return fail_with<int>("at once");
}

/// An awaitable that merely names a task among its template arguments, which
/// is enough for argument-dependent lookup to bring the task's own co_await.
template <class T>
struct names_a_task {
    bool await_ready() const noexcept { return true; }
    void await_suspend(std::coroutine_handle<>) const noexcept {}
    int await_resume() const noexcept { return 9; }
};

task<> awaits_what_names_a_task(int& got) {
    names_a_task<task<>> awaitable;
    got = co_await awaitable;
}

/// Drops a suspended task and sweeps an ended one while running: neither is
/// on the stack, so both may go.
task<> drops_others(std::coroutine_handle<>& slot, std::optional<task<>>& suspended,
                    std::vector<task<>>& ended) {
    co_await parked{slot};
    suspended.reset();
    ended.clear();
}

/// Drops the coroutine that joins it, while running: that one is suspended.
task<int> drops_its_joiner(std::coroutine_handle<>& slot, std::optional<task<>>& joiner) {
    co_await parked{slot};
    joiner.reset();
    co_return 5;
}

}  // namespace

TEST(TaskTest, RunsAsFarAsTheFirstSuspension) {
    int sink = 0;
    task<> t = suspends_once(sink);

    EXPECT_EQ(sink, 1);
    EXPECT_FALSE(t.done());
}

TEST(TaskTest, IsDoneAfterAPlainReturn) {
    int sink = 0;
    task<> t = returns_at_once(sink);

    EXPECT_EQ(sink, 1);
    EXPECT_TRUE(t.done());
    EXPECT_NO_THROW(t.result());
}

TEST(TaskTest, ResultRethrowsWhatLeftTheCoroutine) {
    task<> t = throws("out of the coroutine");

    EXPECT_TRUE(t.done());
    EXPECT_THROW(t.result(), std::runtime_error);
}

TEST(TaskTest, TheValueComesBackThroughCoAwait) {
    std::coroutine_handle<> slot;
    int got = 0, deaths = 0;
    task<> outer = awaits_the_pause(slot, got, deaths);

    EXPECT_FALSE(outer.done());
    ASSERT_TRUE(slot);

    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(got, 42);
    EXPECT_NO_THROW(outer.result());
}

TEST(TaskTest, ATaskAlreadyEndedIsAwaitedWithoutSuspending) {
    int got = 0;
    task<> outer = awaits_at_once(got);

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(got, 7);
}

TEST(TaskTest, TheExceptionCrossesTheCoAwait) {
    std::coroutine_handle<> slot;
    std::string what;
    task<> outer = catches_from_the_inner(slot, what);

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(what, "inner");
    EXPECT_NO_THROW(outer.result());
}

TEST(TaskTest, AChainEndsLinkByLink) {
    std::coroutine_handle<> slot;
    int got = 0, deaths = 0;
    task<> outer = awaits_a_chain(slot, got, deaths);

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(got, 43);
}

TEST(TaskTest, ResultMovesTheValueOut) {
    task<std::unique_ptr<int>> t = hands_over_ownership();

    ASSERT_TRUE(t.done());
    std::unique_ptr<int> p = t.result();
    ASSERT_TRUE(p);
    EXPECT_EQ(*p, 5);
}

TEST(TaskTest, DroppingTheOuterTakesTheInnerDownWithIt) {
    std::coroutine_handle<> slot;
    int got = 0, deaths = 0;
    {
        task<> outer = awaits_the_pause(slot, got, deaths);
        ASSERT_TRUE(slot);
        EXPECT_EQ(deaths, 0);
    }

    // Both frames are gone, the inner one among the outer's locals, and nothing
    // was resumed on the way: `got` never changed hands.
    EXPECT_EQ(deaths, 2);
    EXPECT_EQ(got, 0);
}

TEST(TaskTest, AnLvalueIsJoined) {
    std::coroutine_handle<> slot;
    int got = 0, deaths = 0, joiner_deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    task<> joiner = joins(kept, got, joiner_deaths);

    EXPECT_FALSE(joiner.done());
    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_TRUE(joiner.done());
    EXPECT_EQ(got, 42);

    // One that has ended is joined without suspending.
    task<int> ended = answers_at_once();
    int got_ended = 0;
    task<> at_once = joins(ended, got_ended, joiner_deaths);

    EXPECT_TRUE(at_once.done());
    EXPECT_EQ(got_ended, 7);
}

TEST(TaskTest, AJoinThatOutlivesItsWaiterResumesNobody) {
    std::coroutine_handle<> slot;
    int got = 0, deaths = 0, joiner_deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    {
        task<> joiner = joins(kept, got, joiner_deaths);
        EXPECT_FALSE(joiner.done());
    }
    EXPECT_EQ(joiner_deaths, 1);

    // The waiter took itself off on its way out: ending, the task hands the
    // thread back here instead of into the frame that is gone.
    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_TRUE(kept.done());
    EXPECT_EQ(got, 0);
    EXPECT_EQ(kept.result(), 42);
}

TEST(TaskTest, TheNextWaiterJoinsOnceTheFirstHasLeft) {
    std::coroutine_handle<> slot;
    int first = 0, second = 0, deaths = 0, joiner_deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    {
        task<> leaves = joins(kept, first, joiner_deaths);
    }
    task<> stays = joins(kept, second, joiner_deaths);

    slot.resume();

    EXPECT_TRUE(stays.done());
    EXPECT_EQ(first, 0);
    EXPECT_EQ(second, 42);
}

TEST(TaskTest, WhatLeftTheCoroutineIsThrownEachTimeItIsAsked) {
    // The value goes once; an exception stays, and so does the answer of a
    // task<>, which has no value to take.
    task<> failed = throws("out of the coroutine");
    int caught = 0;
    task<> twice = awaits_twice(failed, caught);

    EXPECT_TRUE(twice.done());
    EXPECT_EQ(caught, 2);
    EXPECT_THROW(failed.result(), std::runtime_error);
    EXPECT_THROW(failed.result(), std::runtime_error);

    task<int> failed_with_a_value = owes_a_value_and_fails();
    EXPECT_THROW((void)failed_with_a_value.result(), std::runtime_error);
    EXPECT_THROW((void)failed_with_a_value.result(), std::runtime_error);
}

TEST(TaskTest, AnAwaitableThatMerelyNamesATaskKeepsItsOwnCoAwait) {
    int got = 0;
    task<> t = awaits_what_names_a_task(got);

    EXPECT_TRUE(t.done());
    EXPECT_EQ(got, 9);
}

TEST(TaskTest, ARunningTaskMayDropTasksThatAreNotOnTheStack) {
    std::coroutine_handle<> own_slot, suspended_slot;
    int sink = 0;
    std::optional<task<>> suspended;
    suspended.emplace(suspends_once(sink));
    std::vector<task<>> ended;
    ended.push_back(returns_at_once(sink));

    task<> dropper = drops_others(own_slot, suspended, ended);
    ASSERT_TRUE(own_slot);
    own_slot.resume();

    EXPECT_TRUE(dropper.done());
    EXPECT_FALSE(suspended.has_value());
    EXPECT_TRUE(ended.empty());
}

TEST(TaskTest, AJoinedTaskMayDropTheCoroutineJoiningIt) {
    std::coroutine_handle<> slot;
    std::optional<task<>> joiner;
    int got = 0, joiner_deaths = 0;
    task<int> kept = drops_its_joiner(slot, joiner);
    joiner.emplace(joins(kept, got, joiner_deaths));

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_FALSE(joiner.has_value());
    EXPECT_EQ(joiner_deaths, 1);
    EXPECT_TRUE(kept.done());
    EXPECT_EQ(got, 0);
    EXPECT_EQ(kept.result(), 5);
}

// The Debug checks cost a Release build nothing: no field in the promise, and
// a task stays one handle wide, as does the awaiter of a joined one.
#ifdef NDEBUG
static_assert(sizeof(task_detail::promise_base) ==
              sizeof(std::coroutine_handle<>) + sizeof(std::exception_ptr));
static_assert(sizeof(task<>) == sizeof(void*) && sizeof(task<int>) == sizeof(void*));
static_assert(sizeof(task_detail::join_awaiter<task_detail::value_promise<int>>) == sizeof(void*));
#endif

// The debug CRT heap is what the probe below reads, and it is only there in a
// debug build.
#ifdef _DEBUG

TEST(TaskTest, TakesItsFrameFromTheStaPool) {
    int sink = 0;

    _CrtMemState before{}, after{}, difference{};

    // The coroutine is left suspended on purpose: a frame that has already been
    // given back would leave the heap exactly as it was found, and the probe
    // would have nothing to see either way.
    _CrtMemCheckpoint(&before);
    task<> t = suspends_once(sink);
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(sink, 1);
    EXPECT_EQ(0, _CrtMemDifference(&difference, &before, &after))
        << "the frame came from the CRT heap rather than from sta_memory_pool";
}

TEST(TaskTest, TheProbeNoticesAFrameFromTheOrdinaryHeap) {
    int sink = 0;

    _CrtMemState before{}, after{}, difference{};

    _CrtMemCheckpoint(&before);
    heap_task t = heap_suspends_once(sink);
    _CrtMemCheckpoint(&after);

    EXPECT_EQ(sink, 1);
    EXPECT_NE(0, _CrtMemDifference(&difference, &before, &after));
}

#endif

// The Debug checks: each misuse ends the process at the check that names it.
#ifndef NDEBUG

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

heap_task joins_from_anywhere(task<int>& kept) {
    (void)co_await kept;
}

task<> drops_the_chain(std::coroutine_handle<>& slot, std::optional<task<>>& chain) {
    co_await parked{slot};
    chain.reset();
}

task<> starts_a_chain(std::coroutine_handle<>& slot, std::optional<task<>>& chain) {
    co_await drops_the_chain(slot, chain);
}

void join_while_another_waits() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int first = 0, second = 0, deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    task<> one = joins(kept, first, deaths);
    task<> two = joins(kept, second, deaths);
}

void take_the_value_twice() {
    report_failures_to_stderr();

    task<int> t = answers_at_once();
    (void)t.result();
    (void)t.result();
}

void ask_before_the_end() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> t = answers_after_a_pause(slot, deaths);
    (void)t.result();
}

void await_from_another_thread() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    std::thread([&kept] { heap_task joiner = joins_from_anywhere(kept); }).join();
}

void drop_the_chain_from_inside() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    std::optional<task<>> chain;
    chain.emplace(starts_a_chain(slot, chain));
    slot.resume();
}

void drop_a_joined_task() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int got = 0, deaths = 0;
    std::optional<task<int>> kept;
    kept.emplace(answers_after_a_pause(slot, deaths));
    task<> joiner = joins(*kept, got, deaths);
    kept.reset();
}

}  // namespace

TEST(TaskDeathTest, ASecondCoroutineMayNotJoinWhileTheFirstWaits) {
    EXPECT_DEATH(join_while_another_waits(), "second coroutine");
}

TEST(TaskDeathTest, TheValueIsTakenOnce) {
    EXPECT_DEATH(take_the_value_twice(), "already been taken");
}

TEST(TaskDeathTest, TheResultIsAskedAfterTheEnd) {
    EXPECT_DEATH(ask_before_the_end(), "before the coroutine ended");
}

TEST(TaskDeathTest, ATaskIsAwaitedOnItsOwnThread) {
    EXPECT_DEATH(await_from_another_thread(), "other than its own");
}

TEST(TaskDeathTest, AChainIsNotDroppedFromInside) {
    EXPECT_DEATH(drop_the_chain_from_inside(), "while its coroutine runs");
}

TEST(TaskDeathTest, AJoinedTaskOutlivesTheWait) {
    EXPECT_DEATH(drop_a_joined_task(), "while a coroutine joins it");
}

#endif
