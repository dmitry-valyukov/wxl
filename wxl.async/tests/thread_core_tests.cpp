#include <gtest/gtest.h>

#include "spin_until.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

TEST(ThreadTest, SpawnedBodyRunsAndItsFutureBecomesReady) {
    thread_scope manager;
    semaphore go(0);
    std::atomic<bool> ran{false};

    future<void> finished = manager->spawn_named(u8"body", [&] {
        ran.store(true);
        go.acquire();
    });

    EXPECT_EQ(future_status::timeout, finished.wait_for(duration::from_ms(10)));

    go.release();

    EXPECT_EQ(future_status::ready, finished.wait_for(duration::from_sec(5)));
    EXPECT_TRUE(finished.has_value());
    EXPECT_TRUE(ran.load());
}

TEST(ThreadTest, ExceptionFromBodyArrivesInTheFuture) {
    thread_scope manager;

    future<void> finished = manager->spawn([] { throw std::domain_error("expected test error"); });

    finished.wait();

    ASSERT_TRUE(finished.has_exception());
    EXPECT_THROW(finished.get(), std::domain_error);
}

TEST(ThreadTest, ScopedThreadCarriesTheSameFuture) {
    joining_thread failing([] { throw std::domain_error("expected test error"); });
    failing.join();

    ASSERT_TRUE(failing.result().is_ready());
    EXPECT_TRUE(failing.result().has_exception());
}

TEST(ThreadTest, DetachedThreadRunsAndReports) {
    std::atomic<bool> ran{false};

    future<void> finished =
        thread_group::spawn_detached_named(u8"detached", [&ran] { ran.store(true); });

    finished.wait();

    EXPECT_TRUE(finished.has_value());
    EXPECT_TRUE(ran.load());
}

TEST(ThreadTest, StaticUtilitiesBehaveSanely) {
    thread::spin_wait(1000);
    thread::spin_wait_nano(500);

    const time_stamp t0 = time_stamp::now();
    thread::spin_wait_micro(100);
    EXPECT_GE((time_stamp::now() - t0).total_microseconds(), 50);

    const auto t1 = std::chrono::steady_clock::now();
    thread::sleep(duration::from_ms(5));
    EXPECT_GE(std::chrono::steady_clock::now() - t1, std::chrono::milliseconds(1));

    int spins = 0;
    spin_until([&spins] { return ++spins >= 3; });
    EXPECT_GE(spins, 3);

    EXPECT_TRUE(spin_until([] { return true; }, duration::from_us(1)));
    EXPECT_FALSE(spin_until([] { return false; }, duration::from_ms(5)));

    EXPECT_THROW(thread::set_affinity(1024), std::exception);

    thread::yield();
}

TEST(ThreadTest, ManyThreadsSpawnAndJoinConcurrently) {
    constexpr int n = 30;

    thread_scope manager;
    semaphore go(0);
    std::atomic<int> ran{0};

    std::vector<future<void>> finished;
    finished.reserve(n);

    for (int i = 0; i < n; ++i)
        finished.push_back(manager->spawn([&] {
            ran.fetch_add(1, std::memory_order_relaxed);
            go.acquire();
        }));

    for (int i = 0; i < n; ++i) go.release();

    for (const future<void>& f : finished)
        EXPECT_EQ(future_status::ready, f.wait_for(duration::from_sec(5)));

    EXPECT_EQ(n, ran.load());
}
