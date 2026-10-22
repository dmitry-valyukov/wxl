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

/// Whether a coroutine with this handle could suspend on this awaiter -- the call the
/// compiler makes for a co_await. A task is awaited only from a coroutine returning a
/// task: from any other, behind an erased handle or a promise of its own, the co_await
/// does not compile.
template <class Awaiter, class Handle>
constexpr bool suspends_from = requires(Awaiter& awaiter, Handle handle) { awaiter.await_suspend(handle); };

using task_handle = std::coroutine_handle<task<>::promise_type>;
using value_task_handle = std::coroutine_handle<task<std::string>::promise_type>;

static_assert(suspends_from<task<int>, task_handle>);
static_assert(suspends_from<task<int>, value_task_handle>);
static_assert(suspends_from<detail::join_awaiter<int>, task_handle>);
static_assert(suspends_from<task<>, value_task_handle>);

static_assert(!suspends_from<task<int>, std::coroutine_handle<>>);
static_assert(!suspends_from<task<int>, heap_task::handle_t>);
static_assert(!suspends_from<detail::join_awaiter<int>, std::coroutine_handle<>>);
static_assert(!suspends_from<detail::join_awaiter<int>, heap_task::handle_t>);
static_assert(!suspends_from<task<>, std::coroutine_handle<>>);

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

/// What result() threw, or nothing if it threw nothing.
template <class Task>
std::string failure_of(Task& t) {
    try {
        t.result();
    } catch (const std::runtime_error& failure) {
        return failure.what();
    }

    return {};
}

/// Fails before its first co_await: by the time the call returns, the task is done
/// and the exception is in its frame.
task<int> fails_at_once(const char* what) {
    co_return fail_with<int>(what);
}

task<> catches_from_an_inner_that_never_suspended(std::string& what) {
    try {
        co_await fails_at_once("before the first co_await");
    } catch (const std::runtime_error& failure) {
        what = failure.what();
    }
}

task<int> passes_on_an_inner_that_never_suspended() {
    co_return 1 + co_await fails_at_once("passed on");
}

task<> passes_on_a_failure_at_once() {
    co_await throws("passed on, without a value");
}

/// What the links of a chain of three saw: the bottom one parks in `slot` and fails
/// once resumed; the middle one stands between it and the top.
struct chain_notes {
    std::coroutine_handle<> slot;
    int deaths = 0;
    int deaths_when_caught = -1;
    bool middle_went_on = false;
    std::string caught;
};

task<std::string> bottom_fails_after_a_pause(chain_notes& notes) {
    notes_death guard{notes.deaths};
    co_await parked{notes.slot};
    co_return fail_with<std::string>("the bottom link");
}

task<std::string> middle_passes_it_on(chain_notes& notes) {
    notes_death guard{notes.deaths};
    std::string got = co_await bottom_fails_after_a_pause(notes);
    notes.middle_went_on = true;
    co_return got;
}

task<std::string> middle_falls_back(chain_notes& notes) {
    notes_death guard{notes.deaths};

    try {
        co_return co_await bottom_fails_after_a_pause(notes);
    } catch (const std::runtime_error& failure) {
        notes.caught = failure.what();
        notes.deaths_when_caught = notes.deaths;
    }

    co_return "the fallback";
}

task<> top_catches(chain_notes& notes) {
    try {
        co_await middle_passes_it_on(notes);
    } catch (const std::runtime_error& failure) {
        notes.caught = failure.what();
        notes.deaths_when_caught = notes.deaths;
    }
}

task<> top_takes_what_it_is_given(chain_notes& notes, std::string& got) {
    got = co_await middle_falls_back(notes);
}

task<std::string> top_passes_it_on_too(chain_notes& notes) {
    co_return co_await middle_passes_it_on(notes);
}

/// An operation in flight, as much of one as a coroutine sees: it parks the coroutine
/// like `parked`, and if it goes while the coroutine is still parked on it, it gives the
/// wait up -- the slot is emptied, so that nothing can resume a frame that is gone. That
/// is what the task of an operation of this module does with it, and what wxl.ui's waits
/// do with their subscriptions.
class operation_in_flight
{
public:
    operation_in_flight(std::coroutine_handle<>& slot, int& given_up) noexcept
        : slot_(slot), given_up_(given_up) {}

    operation_in_flight(const operation_in_flight&) = delete;
    operation_in_flight& operator=(const operation_in_flight&) = delete;

    ~operation_in_flight() {
        if (!waiting_) return;

        slot_ = {};
        ++given_up_;
    }

    bool await_ready() const noexcept { return false; }

    void await_suspend(std::coroutine_handle<> here) noexcept {
        slot_ = here;
        waiting_ = true;
    }

    void await_resume() noexcept { waiting_ = false; }

private:
    std::coroutine_handle<>& slot_;
    int& given_up_;
    bool waiting_ = false;
};

/// Two tasks in flight and what happened to them, in the order it happened.
struct flight_log {
    std::coroutine_handle<> a;
    std::coroutine_handle<> b;
    std::vector<std::string> events;
    int given_up = 0;
    int deaths = 0;
};

task<int> flies(char name, std::coroutine_handle<>& slot, flight_log& log, int value) {
    notes_death guard{log.deaths};
    log.events.push_back(std::string{name} + " starts");
    co_await operation_in_flight{slot, log.given_up};
    log.events.push_back(std::string{name} + " lands");
    co_return value;
}

task<int> fails_in_flight(std::coroutine_handle<>& slot, flight_log& log) {
    notes_death guard{log.deaths};
    log.events.push_back("a starts");
    co_await operation_in_flight{slot, log.given_up};
    log.events.push_back("a fails");
    co_return fail_with<int>("the first in flight");
}

/// Both started before either is awaited, then awaited one after the other: "all of
/// them" with nothing but the eager start.
task<> awaits_two_in_turn(flight_log& log, int& sum) {
    auto a = flies('a', log.a, log, 1);
    auto b = flies('b', log.b, log, 2);
    log.events.push_back("both started");

    const int first = co_await a;
    log.events.push_back("outer has a");

    const int second = co_await b;
    log.events.push_back("outer has b");

    sum = first + second;
}

task<> awaits_a_failure_then_another(flight_log& log) {
    auto a = fails_in_flight(log.a, log);
    auto b = flies('b', log.b, log, 2);

    co_await a;
    log.events.push_back("outer went on");

    co_await b;
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
task<> drops_others(std::coroutine_handle<>& slot, task<>& suspended, std::vector<task<>>& ended) {
    co_await parked{slot};
    suspended = {};
    ended.clear();
}

/// Drops the coroutine that joins it, while running: that one is suspended.
task<int> drops_its_joiner(std::coroutine_handle<>& slot, task<>& joiner) {
    co_await parked{slot};
    joiner = {};
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

// An inner task that fails before its first co_await is done when its call returns,
// so the co_await takes the exception from its frame without suspending -- and the
// outer, which never suspended either, is done by the time its own call returns.
TEST(TaskTest, AnExceptionBeforeTheFirstSuspensionCrossesTheCoAwait) {
    std::string what;
    task<> outer = catches_from_an_inner_that_never_suspended(what);

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(what, "before the first co_await");
    EXPECT_NO_THROW(outer.result());
}

TEST(TaskTest, AnExceptionBeforeTheFirstSuspensionIsPassedOnUncaught) {
    task<int> with_value = passes_on_an_inner_that_never_suspended();
    task<> without = passes_on_a_failure_at_once();

    EXPECT_TRUE(with_value.done());
    EXPECT_EQ(failure_of(with_value), "passed on");

    EXPECT_TRUE(without.done());
    EXPECT_EQ(failure_of(without), "passed on, without a value");
}

// Three links, the bottom one failing once resumed: the exception climbs link by link,
// and every link it passes unwinds -- its locals go -- before the next one up sees it.
TEST(TaskTest, AnExceptionClimbsTwoLinksToTheTopThatCatchesIt) {
    chain_notes notes;
    task<> top = top_catches(notes);

    ASSERT_TRUE(notes.slot);
    EXPECT_FALSE(top.done());

    notes.slot.resume();

    EXPECT_TRUE(top.done());
    EXPECT_NO_THROW(top.result());
    EXPECT_EQ(notes.caught, "the bottom link");
    EXPECT_FALSE(notes.middle_went_on);
    EXPECT_EQ(notes.deaths_when_caught, 2);
}

TEST(TaskTest, ALinkInTheMiddleCatchesAndTheTopGetsAValue) {
    chain_notes notes;
    std::string got;
    task<> top = top_takes_what_it_is_given(notes, got);

    ASSERT_TRUE(notes.slot);
    notes.slot.resume();

    EXPECT_TRUE(top.done());
    EXPECT_NO_THROW(top.result());
    EXPECT_EQ(notes.caught, "the bottom link");

    // The bottom link had unwound by then; the middle one was still running.
    EXPECT_EQ(notes.deaths_when_caught, 1);
    EXPECT_EQ(notes.deaths, 2);
    EXPECT_EQ(got, "the fallback");
}

TEST(TaskTest, AnExceptionNobodyCatchesReachesWhoeverHoldsTheTop) {
    chain_notes notes;
    task<std::string> top = top_passes_it_on_too(notes);

    ASSERT_TRUE(notes.slot);
    notes.slot.resume();

    EXPECT_TRUE(top.done());
    EXPECT_EQ(notes.deaths, 2);
    EXPECT_EQ(failure_of(top), "the bottom link");
}

// Two tasks in flight, awaited in turn. Both run to their first suspension at their
// call, before the outer awaits either.
TEST(TaskTest, TwoInFlightStartInTheOrderTheyAreCalled) {
    flight_log log;
    int sum = 0;
    task<> outer = awaits_two_in_turn(log, sum);

    ASSERT_TRUE(log.a);
    ASSERT_TRUE(log.b);
    EXPECT_FALSE(outer.done());
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "both started"}));

    log.a.resume();

    EXPECT_FALSE(outer.done());
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "both started",
                                                    "a lands", "outer has a"}));

    log.b.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_NO_THROW(outer.result());
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "both started",
                                                    "a lands", "outer has a", "b lands",
                                                    "outer has b"}));
    EXPECT_EQ(sum, 3);
    EXPECT_EQ(log.given_up, 0);
}

// The second one lands first, with nobody awaiting it yet: it hands the thread back to
// whoever resumed it, and the outer, still on the first, is not woken. Awaited later,
// it is done and is not waited for.
TEST(TaskTest, TwoInFlightMayEndInTheOtherOrder) {
    flight_log log;
    int sum = 0;
    task<> outer = awaits_two_in_turn(log, sum);

    ASSERT_TRUE(log.b);
    log.b.resume();

    EXPECT_FALSE(outer.done());
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "both started",
                                                    "b lands"}));

    ASSERT_TRUE(log.a);
    log.a.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_NO_THROW(outer.result());
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "both started",
                                                    "b lands", "a lands", "outer has a",
                                                    "outer has b"}));
    EXPECT_EQ(sum, 3);
}

// The first one awaited fails, and the outer's frame unwinds past the second, still in
// flight: it is destroyed where it stands, and what it was waiting on is given up. It
// never ended, so its locals going is its frame going.
TEST(TaskTest, AFailureOfTheFirstInFlightTakesTheSecondDown) {
    flight_log log;
    task<> outer = awaits_a_failure_then_another(log);

    ASSERT_TRUE(log.a);
    ASSERT_TRUE(log.b);

    log.a.resume();

    EXPECT_TRUE(outer.done());
    EXPECT_EQ(failure_of(outer), "the first in flight");

    EXPECT_EQ(log.deaths, 2);
    EXPECT_EQ(log.given_up, 1);
    EXPECT_FALSE(log.b) << "the second one's wait was not given up";
    EXPECT_EQ(log.events, (std::vector<std::string>{"a starts", "b starts", "a fails"}));
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

// How a task ended, asked without throwing: a value or an exception, exactly one of the two,
// and the exception itself; asking does not take anything, and result() answers after it as
// before.
TEST(TaskTest, HowItEndedIsAskedWithoutThrowing) {
    int sink = 0;
    task<> plain = returns_at_once(sink);
    EXPECT_TRUE(plain.has_value());
    EXPECT_FALSE(plain.has_exception());

    task<int> answered = answers_at_once();
    EXPECT_TRUE(answered.has_value());
    EXPECT_FALSE(answered.has_exception());
    EXPECT_EQ(answered.result(), 7);
    EXPECT_TRUE(answered.has_value()) << "the value taken, the task still ended with it";

    task<int> failed = fails_at_once("at once");
    EXPECT_FALSE(failed.has_value());
    EXPECT_TRUE(failed.has_exception());
    try {
        std::rethrow_exception(failed.get_exception_ptr());
    } catch (const std::runtime_error& failure) {
        EXPECT_STREQ(failure.what(), "at once");
    }
    EXPECT_EQ(failed.get_exception_ptr(), failed.get_exception_ptr()) << "asked again, the same exception";
    EXPECT_THROW((void)failed.result(), std::runtime_error);
}

// A failure after a pause is there once the body has ended, and read as it is: nobody has
// to rethrow it to look at it.
TEST(TaskTest, AFailureAfterAPauseIsReadOnceTheBodyHasEnded) {
    std::coroutine_handle<> slot;
    task<std::string> t = fails_after_a_pause(slot);
    EXPECT_FALSE(t.done());

    slot.resume();

    ASSERT_TRUE(t.done());
    EXPECT_TRUE(t.has_exception());
    EXPECT_FALSE(t.has_value());
    EXPECT_TRUE(t.get_exception_ptr());
}

// Asked before the end, a build that checks stops (HowItEndedIsAskedAfterTheEnd); one that
// does not says the truth of the moment: neither a value nor an exception yet.
TEST(TaskTest, BeforeTheEndItHasNeitherWhereNothingChecks) {
    if constexpr (detail::checked) GTEST_SKIP() << "a build that checks stops there";

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> t = answers_after_a_pause(slot, deaths);
    EXPECT_FALSE(t.has_value());
    EXPECT_FALSE(t.has_exception());

    slot.resume();
    EXPECT_TRUE(t.has_value());
    EXPECT_FALSE(t.has_exception());
}

// An empty task holds nothing -- made empty, or moved from -- and one with work holds it.
TEST(TaskTest, OnlyATaskWithWorkInItHoldsSomething) {
    task<int> empty;
    EXPECT_FALSE(empty);

    task<int> t = answers_at_once();
    EXPECT_TRUE(t);

    task<int> moved = std::move(t);
    EXPECT_FALSE(t);
    EXPECT_TRUE(moved);
    EXPECT_EQ(moved.result(), 7);
}

// Assigned over, a task drops the one it held there and then, as its destructor would, and the
// one it took its task from is left empty rather than holding the old one.
TEST(TaskTest, AssigningOverATaskDropsTheOneItHeld) {
    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> field = answers_after_a_pause(slot, deaths);
    task<int> next = answers_at_once();

    field = std::move(next);

    EXPECT_EQ(deaths, 1) << "the task assigned over lives on";
    EXPECT_FALSE(next);
    EXPECT_EQ(field.result(), 7);

    // Assigned an empty one, it is empty too, and drops what it held the same way.
    field = {};
    EXPECT_FALSE(field);
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
    task<> suspended = suspends_once(sink);
    std::vector<task<>> ended;
    ended.push_back(returns_at_once(sink));

    task<> dropper = drops_others(own_slot, suspended, ended);
    ASSERT_TRUE(own_slot);
    own_slot.resume();

    EXPECT_TRUE(dropper.done());
    EXPECT_FALSE(suspended);
    EXPECT_TRUE(ended.empty());
}

TEST(TaskTest, AJoinedTaskMayDropTheCoroutineJoiningIt) {
    std::coroutine_handle<> slot;
    task<> joiner;
    int got = 0, joiner_deaths = 0;
    task<int> kept = drops_its_joiner(slot, joiner);
    joiner = joins(kept, got, joiner_deaths);

    ASSERT_TRUE(slot);
    slot.resume();

    EXPECT_FALSE(joiner);
    EXPECT_EQ(joiner_deaths, 1);
    EXPECT_TRUE(kept.done());
    EXPECT_EQ(got, 0);
    EXPECT_EQ(kept.result(), 5);
}

// What a producer keeps is destroyed once, by the producer's destructor, and only what it
// holds: the error if one is kept, else the value if the body ended, else nothing.

namespace {

/// An exception that counts its own copies: what an exception_ptr keeps alive.
class counted_failure : public std::runtime_error
{
public:
    static inline int live = 0;

    counted_failure() : std::runtime_error("counted") { ++live; }
    counted_failure(const counted_failure& other) : std::runtime_error(other) { ++live; }
    ~counted_failure() override { --live; }
};

/// A value that counts itself, and stops at being destroyed twice.
class counted_value
{
public:
    static inline int live = 0;
    static inline int destroyed_twice = 0;

    counted_value() noexcept { ++live; }
    counted_value(counted_value&&) noexcept { ++live; }
    counted_value& operator=(counted_value&&) = delete;

    ~counted_value() {
        if (state_ != constructed) ++destroyed_twice;
        state_ = gone;
        --live;
    }

private:
    static constexpr std::uint32_t constructed = 0x600dc0de;
    static constexpr std::uint32_t gone = 0xdeadbeef;

    std::uint32_t state_ = constructed;
};

/// Both counts back at zero before each test, and checked after it.
class counts_balanced
{
public:
    counts_balanced() noexcept {
        counted_failure::live = 0;
        counted_value::live = 0;
        counted_value::destroyed_twice = 0;
    }

    counts_balanced(const counts_balanced&) = delete;
    counts_balanced& operator=(const counts_balanced&) = delete;

    ~counts_balanced() {
        EXPECT_EQ(counted_failure::live, 0) << "an exception was kept past its producer";
        EXPECT_EQ(counted_value::live, 0) << "a value was kept past its producer";
        EXPECT_EQ(counted_value::destroyed_twice, 0);
    }
};

[[noreturn]] counted_value fail_counted() {
    throw counted_failure();
}

task<counted_value> fails_counted() {
    co_return fail_counted();
}

task<counted_value> answers_counted_after_a_pause(std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return counted_value{};
}

task<counted_value> fails_counted_after_a_pause(std::coroutine_handle<>& slot) {
    co_await parked{slot};
    co_return fail_counted();
}

/// A producer of this thread whose answer is set by hand, to hand it on to another.
template <class R>
class answered_here final : public async_op_t<R>
{
public:
    void answer_value() requires(!std::is_void_v<R>) { this->set_value(R{}); }

    void answer_failure() {
        try {
            throw counted_failure();
        } catch (...) {
            this->set_current_exception();
        }
    }

    void take_from(answered_here& made) noexcept { this->take_answer_from(made); }

    void end() noexcept { this->complete_in_place(); }

protected:
    bool execute() override { return true; }
};

/// Deletes a producer of the test as a task's last owner does once it is ready; one not
/// ready -- an orphan whose answer went elsewhere -- is deleted as the loop deletes it.
template <class R>
using producer_ptr = std::unique_ptr<answered_here<R>>;

}  // namespace

TEST(TaskTest, AFailureIsReleasedWithItsTaskAndNothingElseIs) {
    counts_balanced counts;

    {
        task<counted_value> failed = fails_counted();

        ASSERT_TRUE(failed.done());
        EXPECT_TRUE(failed.has_exception());
        EXPECT_GE(counted_failure::live, 1) << "the exception is kept";
        EXPECT_EQ(counted_value::live, 0);
        EXPECT_THROW((void)failed.result(), counted_failure);
    }

    std::coroutine_handle<> slot;
    {
        task<counted_value> failed = fails_counted_after_a_pause(slot);
        slot.resume();

        ASSERT_TRUE(failed.done());
        EXPECT_GE(counted_failure::live, 1);
    }
}

TEST(TaskTest, AValueIsDestroyedOnceWhenItsTaskGoesAndNeverBeforeTheCoReturn) {
    counts_balanced counts;
    std::coroutine_handle<> slot;

    {
        task<counted_value> answered = answers_counted_after_a_pause(slot);

        EXPECT_EQ(counted_value::live, 0) << "a value before the co_return";

        slot.resume();

        ASSERT_TRUE(answered.done());
        EXPECT_EQ(counted_value::live, 1) << "the value is kept, and only it";
    }

    EXPECT_EQ(counted_value::live, 0);

    {
        task<counted_value> answered = answers_counted_after_a_pause(slot);
        slot.resume();

        {
            counted_value taken = answered.result();
            EXPECT_EQ(counted_value::live, 2) << "the value taken, and what it was moved out of";
        }

        EXPECT_EQ(counted_value::live, 1);
    }
}

TEST(TaskTest, ABodyStoppedInFlightLeavesNeitherAValueNorAnErrorToDestroy) {
    counts_balanced counts;
    std::coroutine_handle<> slot;

    {
        task<counted_value> answers = answers_counted_after_a_pause(slot);
        EXPECT_FALSE(answers.done());
    }

    {
        task<counted_value> fails = fails_counted_after_a_pause(slot);
        EXPECT_FALSE(fails.done());
    }

    EXPECT_EQ(counted_value::live, 0);
    EXPECT_EQ(counted_failure::live, 0);
}

TEST(TaskTest, AnAnswerHandedOnLeavesTheProducerItCameFrom) {
    counts_balanced counts;

    // A value, there and back: the one that hands it on is left with nothing, and its
    // destructor, finding it not ready, destroys nothing.
    {
        producer_ptr<counted_value> made(new answered_here<counted_value>);
        producer_ptr<counted_value> waiting(new answered_here<counted_value>);

        made->answer_value();
        waiting->take_from(*made);

        EXPECT_EQ(counted_value::live, 1);
        EXPECT_FALSE(made->has_exception());

        made->take_from(*waiting);
        made->end();

        EXPECT_EQ(counted_value::live, 1);
        EXPECT_FALSE(made->has_exception());
    }

    EXPECT_EQ(counted_value::live, 0);

    // A failure, there and back: its bytes move, and it is released once, by its last holder.
    {
        producer_ptr<counted_value> made(new answered_here<counted_value>);
        producer_ptr<counted_value> waiting(new answered_here<counted_value>);

        made->answer_failure();
        const int kept = counted_failure::live;
        waiting->take_from(*made);

        EXPECT_FALSE(made->has_exception());
        EXPECT_TRUE(waiting->has_exception());
        EXPECT_EQ(counted_failure::live, kept);

        made->take_from(*waiting);
        made->end();

        EXPECT_TRUE(made->has_exception());
        EXPECT_FALSE(waiting->has_exception());
        EXPECT_EQ(counted_value::live, 0);
    }

    EXPECT_EQ(counted_failure::live, 0);

    // Without a value, the failure the same way.
    {
        producer_ptr<void> made(new answered_here<void>);
        producer_ptr<void> waiting(new answered_here<void>);

        made->answer_failure();
        waiting->take_from(*made);
        waiting->end();

        EXPECT_FALSE(made->has_exception());
        EXPECT_TRUE(waiting->has_exception());
    }

    EXPECT_EQ(counted_failure::live, 0);
}

// The producer of a task's coroutine is a producer like an operation -- the virtual table,
// the waiter's handle, a word of flags and the exception's storage, then the value's -- and
// the handle of the frame while the body is in flight; the promise is one pointer to it. The
// checks cost it nothing in any build, strict or not: what they keep -- whether the body is
// on the stack, whether the waiter joined -- is two bits of the waiter's handle, and whether
// the value has been taken, and whether it is held, are two bytes in the padding of the flags'
// word. A task stays one pointer wide, as does the awaiter of a joined one. (The
// exception is two pointers wide on MSVC, one on g++ and clang.) The value is stored as
// itself, with no flag of its own: an int takes what alignment leaves of a word, and a
// size_t a word.
static_assert(sizeof(async_op) == 3 * sizeof(void*) + sizeof(std::exception_ptr));
static_assert(sizeof(async_op_t<int>) == sizeof(async_op) + sizeof(void*));
static_assert(sizeof(async_op_t<std::size_t>) == sizeof(async_op) + sizeof(std::size_t));
static_assert(sizeof(detail::coroutine_op<void>) == sizeof(async_op) + sizeof(void*));
static_assert(sizeof(detail::coroutine_op<int>) == sizeof(async_op_t<int>) + sizeof(void*));
static_assert(sizeof(detail::task_promise<void>) == sizeof(void*));
static_assert(sizeof(detail::task_promise<int>) == sizeof(void*));
static_assert(sizeof(task<>) == sizeof(void*) && sizeof(task<int>) == sizeof(void*));
static_assert(sizeof(detail::join_awaiter<int>) == sizeof(void*));

// What a build that checks does pay, in the frame of a task: the awaiter of each co_await
// in its body is wrapped, one reference more.
static_assert(sizeof(detail::tracked_awaiter<task<int>&>) == 2 * sizeof(void*));

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

// The checks: each misuse ends the process at the check that names it -- assert in a
// Debug build, core::abort under STRICT_CORO in any build. A build with neither has
// nothing to stop at, and the misuse is undefined behaviour, so it is not run there.

namespace {

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

/// Takes the coroutine to a thread of its own and resumes it there, waiting for it to
/// suspend again or end: an awaitable of somebody else's that moves a body off its thread,
/// as a hop to a thread pool would.
struct to_another_thread {
    bool await_ready() const noexcept { return false; }
    void await_suspend(std::coroutine_handle<> here) const { std::thread([here] { here.resume(); }).join(); }
    void await_resume() const noexcept {}
};

task<> drops_the_chain(std::coroutine_handle<>& slot, task<>& chain) {
    co_await parked{slot};
    chain = {};
}

task<> starts_a_chain(std::coroutine_handle<>& slot, task<>& chain) {
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

void ask_how_it_ended_before_the_end() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> t = answers_after_a_pause(slot, deaths);
    (void)t.has_exception();
}

void ask_for_the_exception_of_a_value() {
    report_failures_to_stderr();

    task<int> t = answers_at_once();
    (void)t.get_exception_ptr();
}

void ask_an_empty_task_how_it_ended() {
    report_failures_to_stderr();

    task<int> t;
    (void)t.has_value();
}

void ask_before_the_end() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> t = answers_after_a_pause(slot, deaths);
    (void)t.result();
}

// A task's coroutine moved to another thread, joining there.
constexpr std::uint_least32_t line_of_the_foreign_join = std::source_location::current().line() + 1;
task<> joins_from_there(task<int>& kept) { co_await to_another_thread{}; (void)co_await kept; }

void await_from_another_thread() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    task<> joiner = joins_from_there(kept);
}

void drop_the_chain_from_inside() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    task<> chain;
    chain = starts_a_chain(slot, chain);
    slot.resume();
}

void drop_a_joined_task() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int got = 0, deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    task<> joiner = joins(kept, got, deaths);
    kept = {};
}

/// The place a strict build's report names: the file's own name and the line, in the
/// form core::abort writes them.
std::string place_of(std::string_view file, std::uint_least32_t line) {
    return std::format("{}\\({}\\): ", file, line);
}

/// The report of a check in task's own code, a destructor, which no line of the caller
/// reaches: the place is wxl's.
std::string in_task(std::string_view why) {
    return std::format("task\\..*\\(.*\\): {}", why);
}

// Each misuse a strict build reports with the caller's line sits alone on the line
// below the one that records it.

constexpr std::uint_least32_t line_of_the_second_take = std::source_location::current().line() + 1;
void take_again(task<int>& t) { (void)t.result(); }

void take_the_value_twice_by_result() {
    report_failures_to_stderr();

    task<int> t = answers_at_once();
    (void)t.result();
    take_again(t);
}

constexpr std::uint_least32_t line_of_the_early_ask = std::source_location::current().line() + 1;
void ask(task<int>& t) { (void)t.result(); }

constexpr std::uint_least32_t line_of_the_wrong_question = std::source_location::current().line() + 1;
void ask_for_the_exception(task<int>& t) { (void)t.get_exception_ptr(); }

void ask_for_the_exception_of_a_value_by_line() {
    report_failures_to_stderr();

    task<int> t = answers_at_once();
    ask_for_the_exception(t);
}

void ask_too_early() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int deaths = 0;
    task<int> t = answers_after_a_pause(slot, deaths);
    ask(t);
}

// A task awaiting a task: the place comes through the awaiter await_transform wraps.
constexpr std::uint_least32_t line_of_the_second_join = std::source_location::current().line() + 1;
task<> joins_second(task<int>& kept) { (void)co_await kept; }

void join_second_while_the_first_waits() {
    report_failures_to_stderr();

    std::coroutine_handle<> slot;
    int first = 0, deaths = 0;
    task<int> kept = answers_after_a_pause(slot, deaths);
    task<> one = joins(kept, first, deaths);
    task<> two = joins_second(kept);
}

// The value taken twice by co_await: the place is the second co_await's.
constexpr std::uint_least32_t line_of_the_second_await = std::source_location::current().line() + 1;
task<> awaits_twice_by_value(task<int>& kept) { (void)co_await kept; (void)co_await kept; }

void take_the_value_twice_by_co_await() {
    report_failures_to_stderr();

    task<int> kept = answers_at_once();
    task<> twice = awaits_twice_by_value(kept);
}

// A task that has been moved from has nothing to wait for: awaiting it fails at the
// co_await, whatever made the result it no longer holds.
constexpr std::uint_least32_t line_of_the_moved_from_await = std::source_location::current().line() + 1;
task<> awaits_what_is_left(task<int>& left) { (void)co_await left; }

void await_a_moved_from_task() {
    report_failures_to_stderr();

    task<int> t = answers_at_once();
    task<int> moved = std::move(t);
    task<> waiter = awaits_what_is_left(t);
}

/// A producer that says it is done with nothing in it, and one that holds a value it never
/// completed with: both answers the checks keep a byte for.
void take_a_result_never_made() {
    report_failures_to_stderr();

    producer_ptr<int> empty(new answered_here<int>);
    empty->end();
    (void)empty->take_result();
}

void drop_a_value_never_completed() {
    report_failures_to_stderr();

    producer_ptr<int> unfinished(new answered_here<int>);
    unfinished->answer_value();
}

/// A local whose destructor throws: past a co_return, the body ends with both a value and an
/// exception.
struct throws_as_it_goes {
    ~throws_as_it_goes() noexcept(false) { throw std::runtime_error("from a destructor"); }
};

task<int> throws_after_its_co_return() {
    throws_as_it_goes local;
    co_return 1;
}

void throw_after_co_return() {
    report_failures_to_stderr();

    task<int> both = throws_after_its_co_return();
}

/// Death tests of the checks: skipped where the build has none.
class TaskDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::checked) GTEST_SKIP() << "built without coroutine checks";
    }
};

/// The report of a strict build: the reason, and the line of the code that broke the
/// rule. Skipped elsewhere: a Debug build's assert names the condition and its own line.
class TaskStrictDeathTest : public ::testing::Test
{
protected:
    void SetUp() override {
        if constexpr (!detail::strict) GTEST_SKIP() << "built without STRICT_CORO";
    }
};

}  // namespace

TEST_F(TaskDeathTest, ASecondCoroutineMayNotJoinWhileTheFirstWaits) {
    EXPECT_DEATH(join_while_another_waits(), "second coroutine");
}

TEST_F(TaskDeathTest, TheValueIsTakenOnce) {
    EXPECT_DEATH(take_the_value_twice(), "already been taken");
}

TEST_F(TaskDeathTest, TheResultIsAskedAfterTheEnd) {
    EXPECT_DEATH(ask_before_the_end(), "before it ended");
}

TEST_F(TaskDeathTest, HowItEndedIsAskedAfterTheEnd) {
    EXPECT_DEATH(ask_how_it_ended_before_the_end(), "how it ended asked before it ended");
}

TEST_F(TaskDeathTest, TheExceptionIsAskedOfATaskThatHasOne) {
    EXPECT_DEATH(ask_for_the_exception_of_a_value(), "ended with its value");
}

TEST_F(TaskDeathTest, AnEmptyTaskHasNotEnded) {
    EXPECT_DEATH(ask_an_empty_task_how_it_ended(), "task: empty -- default-constructed or moved-from");
}

TEST_F(TaskDeathTest, ATaskIsAwaitedOnItsOwnThread) {
    EXPECT_DEATH(await_from_another_thread(), "other than its own");
}

TEST_F(TaskDeathTest, AChainIsNotDroppedFromInside) {
    EXPECT_DEATH(drop_the_chain_from_inside(), "while its coroutine runs");
}

TEST_F(TaskDeathTest, AJoinedTaskOutlivesTheWait) {
    EXPECT_DEATH(drop_a_joined_task(), "while a coroutine joins it");
}

TEST_F(TaskDeathTest, AMovedFromTaskIsNotAwaited) {
    EXPECT_DEATH(await_a_moved_from_task(), "task: empty -- default-constructed or moved-from");
}

TEST_F(TaskDeathTest, AProducerDoneWithoutItsValueStopsTheTake) {
    EXPECT_DEATH(take_a_result_never_made(), "task: ended without a result");
}

TEST_F(TaskDeathTest, AValueHeldByAProducerThatIsNotReadyStopsItsDestruction) {
    EXPECT_DEATH(drop_a_value_never_completed(), "holds its value, and no other does");
}

TEST_F(TaskDeathTest, ADestructorThatThrowsAfterTheCoReturnStops) {
    EXPECT_DEATH(throw_after_co_return(), "task: a destructor threw after co_return");
}

TEST_F(TaskStrictDeathTest, ResultNamesTheLineThatAskedTwice) {
    EXPECT_DEATH(take_the_value_twice_by_result(),
                 place_of("task_tests\\.cpp", line_of_the_second_take) + "task: the value has already been taken");
}

TEST_F(TaskStrictDeathTest, ResultNamesTheLineThatAskedTooEarly) {
    EXPECT_DEATH(ask_too_early(),
                 place_of("task_tests\\.cpp", line_of_the_early_ask) + "task: result\\(\\) asked before it ended");
}

TEST_F(TaskStrictDeathTest, GetExceptionPtrNamesTheLineThatAsked) {
    EXPECT_DEATH(ask_for_the_exception_of_a_value_by_line(),
                 place_of("task_tests\\.cpp", line_of_the_wrong_question) +
                     "task: get_exception_ptr\\(\\) asked of a task that ended with its value");
}

TEST_F(TaskStrictDeathTest, ATaskAwaitingNamesItsCoAwait) {
    EXPECT_DEATH(join_second_while_the_first_waits(),
                 place_of("task_tests\\.cpp", line_of_the_second_join) +
                     "task: awaited by a second coroutine while the first one still waits");
}

TEST_F(TaskStrictDeathTest, ASecondCoAwaitNamesItsLine) {
    EXPECT_DEATH(take_the_value_twice_by_co_await(),
                 place_of("task_tests\\.cpp", line_of_the_second_await) + "task: the value has already been taken");
}

TEST_F(TaskStrictDeathTest, AForeignThreadNamesTheCoAwait) {
    EXPECT_DEATH(await_from_another_thread(),
                 place_of("task_tests\\.cpp", line_of_the_foreign_join) + "task: awaited from a thread other than its own");
}

TEST_F(TaskStrictDeathTest, AMovedFromTaskNamesTheCoAwait) {
    EXPECT_DEATH(await_a_moved_from_task(),
                 place_of("task_tests\\.cpp", line_of_the_moved_from_await) +
                     "task: empty -- default-constructed or moved-from");
}

TEST_F(TaskStrictDeathTest, ADestructorNamesTheReasonAndItsOwnPlace) {
    EXPECT_DEATH(drop_a_joined_task(), in_task("task: destroyed while a coroutine joins it"));
    EXPECT_DEATH(drop_the_chain_from_inside(),
                 in_task("task: destroyed while its coroutine runs -- dropped from inside its own chain"));
}
