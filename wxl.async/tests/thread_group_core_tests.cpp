#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

TEST(ThreadGroupTest, SpawnedThreadsRunAndJoinAll) {
    thread_scope manager;
    std::atomic<int> counter{0};

    for (int i = 0; i < 50; ++i)
        manager->spawn([&] { counter.fetch_add(1, std::memory_order_relaxed); });

    manager->join_all();
    EXPECT_EQ(counter.load(), 50);
}

TEST(ThreadGroupTest, SpawnNamedThreadRuns) {
    thread_scope manager;
    std::atomic<bool> ran{false};

    manager->spawn_named(u8"diag-worker", [&] { ran.store(true); });
    manager->join_all();

    EXPECT_TRUE(ran.load());
}

TEST(ThreadGroupTest, CloseJoinsAllAndDisallowsFurtherSpawn) {
    thread_scope manager;
    std::atomic<int> counter{0};

    for (int i = 0; i < 20; ++i)
        manager->spawn([&] { counter.fetch_add(1, std::memory_order_relaxed); });

    manager->close();
    EXPECT_EQ(counter.load(), 20);

    EXPECT_THROW(manager->spawn([] {}), std::logic_error);
}

TEST(ThreadGroupTest, JoinAllFromASpawnedThreadThrows) {
    thread_scope manager;
    std::atomic<bool> threw{false};

    manager->spawn([&] {
        try {
            manager->join_all();
        } catch (const std::logic_error &) {
            threw.store(true);
        }
    });

    manager->join_all();
    EXPECT_TRUE(threw.load());
}

TEST(ThreadGroupTest, ScopedThreadRunsAndJoins) {
    std::atomic<bool> ran{false};
    joining_thread t([&] { ran.store(true); });
    t.join();
    EXPECT_TRUE(ran.load());
}

TEST(ThreadGroupTest, DetachedThreadsAreWaitedOnViaStaticApi) {
    std::atomic<int> counter{0};
    constexpr int n = 30;

    for (int i = 0; i < n; ++i) {
        thread_group::spawn_detached([&] {
            std::this_thread::sleep_for(std::chrono::milliseconds(5));
            counter.fetch_add(1, std::memory_order_relaxed);
        });
    }

    ASSERT_TRUE(thread_group::join_detached_threads_for(duration::from_ms(5000)));
    EXPECT_EQ(counter.load(), n);
    EXPECT_EQ(thread_group::detached_thread_count(), 0u);
}

TEST(ThreadGroupTest, IsInternalThreadDistinguishesManagedFromExternalThreads) {
    thread_scope manager;
    std::atomic<bool> managed_result{false};

    manager->spawn([&] { managed_result.store(thread_group::is_internal_thread()); });
    manager->join_all();
    EXPECT_TRUE(managed_result.load());

    std::atomic<bool> external_result{true};
    std::thread external([&] { external_result.store(thread_group::is_internal_thread()); });
    external.join();
    EXPECT_FALSE(external_result.load());
}

TEST(ThreadGroupTest, AtThreadExitCallbackRunsOnThreadExit) {
    thread_scope manager;
    std::atomic<int> value{0};

    manager->spawn([&] {
        thread_group::at_thread_exit(+[](void * arg) {
            static_cast<std::atomic<int> *>(arg)->fetch_add(1, std::memory_order_relaxed);
        }, &value);
    });

    manager->join_all();
    EXPECT_EQ(value.load(), 1);
}

// Nested managers spawned concurrently from many threads, racing thread_info_container's
// add/remove/join bookkeeping under real contention.
TEST(ThreadGroupTest, ConcurrentNestedManagersHandleEveryTask) {
    constexpr int outer_threads = 8;
    constexpr int inner_spawns = 200;
    std::atomic<int> total{0};

    std::vector<std::thread> outers;
    for (int i = 0; i < outer_threads; ++i) {
        outers.emplace_back([&] {
            thread_scope manager;
            for (int j = 0; j < inner_spawns; ++j)
                manager->spawn([&] { total.fetch_add(1, std::memory_order_relaxed); });
            manager->join_all();
        });
    }

    for (auto & t : outers) t.join();
    EXPECT_EQ(total.load(), outer_threads * inner_spawns);
}
