#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

TEST(TurnstileTest, ClosingAnEmptyTurnstileSucceedsImmediately) {
    turnstile gate;

    EXPECT_FALSE(gate.closed());
    EXPECT_TRUE(gate.try_close());
    EXPECT_TRUE(gate.closed());
    EXPECT_FALSE(gate.try_enter());
    EXPECT_TRUE(gate.closed());
}

TEST(TurnstileTest, ClosingFailsWhileSomeoneIsInsideAndSucceedsOnceEmpty) {
    turnstile gate;

    for (size_t i = 1; i < 100; ++i) {
        EXPECT_TRUE(gate.try_enter());
        EXPECT_FALSE(gate.try_close());
        gate.exit();
        EXPECT_FALSE(gate.closed());
    }

    EXPECT_TRUE(gate.try_close());
    EXPECT_TRUE(gate.closed());
}

TEST(TurnstileTest, ClosingWaitsForEveryEntryToLeaveNotJustTheLast) {
    // What the turnstile keeps is a count, not a flag: three entries need three exits.
    turnstile gate;

    ASSERT_TRUE(gate.try_enter());
    ASSERT_TRUE(gate.try_enter());
    ASSERT_TRUE(gate.try_enter());

    gate.exit();
    EXPECT_FALSE(gate.try_close());
    gate.exit();
    EXPECT_FALSE(gate.try_close());

    gate.exit();
    EXPECT_TRUE(gate.try_close());
}

TEST(TurnstileTest, ClosingAnAlreadyClosedTurnstileReportsSuccess) {
    turnstile gate;

    ASSERT_TRUE(gate.try_close());
    EXPECT_TRUE(gate.try_close());
    EXPECT_TRUE(gate.closed());
}

TEST(TurnstileTest, ARefusedEntryLeavesTheTurnstileClosed) {
    // A failed try_enter() bumps the counter and takes the bump back. The counter has to
    // land where it started, or a later entry would find the turnstile open again.
    turnstile gate;
    ASSERT_TRUE(gate.try_close());

    for (int i = 0; i < 1000; ++i) EXPECT_FALSE(gate.try_enter());

    EXPECT_TRUE(gate.closed());
    EXPECT_TRUE(gate.try_close());
}

TEST(TurnstileTest, ConcurrentEntriesAllSucceedAndClosingWaitsForAllToLeave) {
    constexpr int users = 16;
    turnstile gate;
    std::atomic<int> entered{0};
    std::barrier enter_sync(users);
    std::barrier exit_sync(users);

    std::vector<std::thread> threads;
    for (int i = 0; i < users; ++i) {
        threads.emplace_back([&] {
            EXPECT_TRUE(gate.try_enter());
            entered.fetch_add(1, std::memory_order_relaxed);

            enter_sync.arrive_and_wait();
            // While every thread is inside, closing must fail.
            EXPECT_FALSE(gate.try_close());
            exit_sync.arrive_and_wait();

            gate.exit();
        });
    }

    for (auto& t : threads) t.join();

    EXPECT_EQ(entered.load(), users);
    EXPECT_FALSE(gate.closed());
    EXPECT_TRUE(gate.try_close());
}

TEST(TurnstileGuardTest, EntersOnConstructionAndLeavesOnDestruction) {
    turnstile gate;

    {
        turnstile_guard pass(gate);
        EXPECT_TRUE(pass.entered());
        EXPECT_FALSE(gate.try_close());  // somebody is inside: the pass itself
    }

    EXPECT_TRUE(gate.try_close());  // and now nobody is
}

TEST(TurnstileGuardTest, AClosedTurnstileRefusesThePassAndTheDestructorDoesNotUnderflow) {
    turnstile gate;
    ASSERT_TRUE(gate.try_close());

    {
        turnstile_guard pass(gate);
        EXPECT_FALSE(pass.entered());
    }

    // An exit() the pass never earned would drive the counter past the sentinel and wrap
    // it positive, reopening the turnstile.
    EXPECT_TRUE(gate.closed());
    EXPECT_FALSE(gate.try_enter());
}

// Mirrors task_queue's own usage: producers race turnstile_guard against a concurrent
// try_close() loop; once closing succeeds, no further entry does.
TEST(TurnstileGuardTest, ClosingEventuallySucceedsUnderConcurrentEntryAttempts) {
    turnstile gate;
    std::atomic<bool> stop{false};
    std::atomic<int> passes{0};

    std::thread worker([&] {
        while (!stop.load(std::memory_order_relaxed)) {
            turnstile_guard pass(gate);

            if (pass.entered()) passes.fetch_add(1, std::memory_order_relaxed);
        }
    });

    // Make sure the worker has actually been through at least once before racing to close,
    // otherwise try_close() on an untouched turnstile can win before the worker is
    // scheduled.
    while (passes.load(std::memory_order_relaxed) == 0)
        std::this_thread::yield();

    while (!gate.try_close())
        std::this_thread::yield();

    stop.store(true, std::memory_order_relaxed);
    worker.join();

    EXPECT_TRUE(gate.closed());
    EXPECT_GT(passes.load(), 0);
}
