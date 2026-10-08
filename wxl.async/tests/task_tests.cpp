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

task<std::string> fails_after_a_pause(std::coroutine_handle<>& slot) {
    co_await parked{slot};
    throw std::runtime_error("inner");
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
