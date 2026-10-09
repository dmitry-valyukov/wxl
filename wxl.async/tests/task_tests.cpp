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
/// is what an awaitable of this module does with its operation, and what wxl.ui's waits
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
