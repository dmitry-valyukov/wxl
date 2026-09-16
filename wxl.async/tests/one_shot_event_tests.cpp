#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

TEST(OneShotEventTest, DefaultConstructedIsNotSignaled) {
    one_shot_event ev;
    EXPECT_FALSE(ev.signaled());
}

TEST(OneShotEventTest, ConstructedSignaledIsImmediatelySignaled) {
    one_shot_event ev(true);
    EXPECT_TRUE(ev.signaled());
    EXPECT_TRUE(ev.wait_for(duration::zero()));
}

TEST(OneShotEventTest, SignalMakesItSignaled) {
    one_shot_event ev;
    ev.signal();
    EXPECT_TRUE(ev.signaled());
}

TEST(OneShotEventTest, WaitOnUnsignaledTimesOut) {
    one_shot_event ev;
    EXPECT_FALSE(ev.wait_for(duration::from_ms(10)));
}

TEST(OneShotEventTest, WaitReturnsTrueOnceSignaled) {
    one_shot_event ev;
    ev.signal();
    EXPECT_TRUE(ev.wait_for(duration::from_ms(1000)));
}

TEST(OneShotEventTest, WaitBlocksUntilSignaledFromAnotherThread) {
    one_shot_event ev;
    std::atomic<bool> waited{false};

    std::thread waiter([&] {
        ev.wait();
        EXPECT_TRUE(ev.signaled());
        waited.store(true);
    });

    std::this_thread::sleep_for(std::chrono::milliseconds(30));
    EXPECT_FALSE(waited.load());

    ev.signal();
    waiter.join();
    EXPECT_TRUE(waited.load());
}

// One-shot: once signaled, it stays signaled forever, so every future wait()
// (concurrent or sequential, using the already-created OS handle or racing to
// create it) must succeed.
TEST(OneShotEventTest, StaysSignaledForAllFutureWaiters) {
    one_shot_event ev;
    ev.signal();

    for (int i = 0; i < 5; ++i)
        EXPECT_TRUE(ev.wait_for(duration::from_ms(1000)));
}

TEST(OneShotEventTest, MultipleConcurrentWaitersAllWakeOnSignal) {
    constexpr int waiter_count = 8;
    one_shot_event ev;
    std::atomic<int> woken{0};
    std::barrier sync_point(waiter_count);

    std::vector<std::thread> threads;
    for (int i = 0; i < waiter_count; ++i) {
        threads.emplace_back([&] {
            sync_point.arrive_and_wait();

            if (ev.wait_for(duration::from_ms(2000)))
                woken.fetch_add(1);
        });
    }

    std::this_thread::sleep_for(std::chrono::milliseconds(20));
    ev.signal();

    for (auto & t : threads) t.join();
    EXPECT_EQ(woken.load(), waiter_count);
}

TEST(OneShotEventTest, SignalIsIdempotent) {
    one_shot_event ev;
    ev.signal();
    ev.signal();
    ev.signal();
    EXPECT_TRUE(ev.signaled());
    EXPECT_TRUE(ev.wait_for(duration::zero()));
}
