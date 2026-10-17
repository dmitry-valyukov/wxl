#include "process_handles.h"

#include <crtdbg.h>

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

// A thread's handle is the group's to close, and joining the thread is what closes it: the
// group lives on, and every thread it has joined is gone from it with its handle.
TEST(ThreadGroupTest, JoinClosesTheHandlesOfJoinedThreads) {
    thread_scope manager;
    constexpr int cycles = 64;

    const long gained = handles_gained(cycles, [&] {
        manager->spawn([] {});
        manager->join_all();
    });

    EXPECT_LT(gained, cycles / 4);
}

// A group let go of without a join goes with its last reference, and closes the handles of
// the threads still on its books. That reference may be the thread's own, given back on its
// way out -- so the body opens its thread for the test to wait on, and the group has gone by
// the time the wait returns.
TEST(ThreadGroupTest, ReleasedGroupClosesTheHandlesOfThreadsNobodyJoined) {
    constexpr int cycles = 64;

    const long gained = handles_gained(cycles, [] {
        HANDLE thread = nullptr;
        thread_group_ptr group = thread_group::create();

        group->spawn([&thread] { thread = open_current_thread(); }).get();
        group.reset();

        ASSERT_NE(thread, nullptr);
        EXPECT_EQ(::WaitForSingleObject(thread, INFINITE), WAIT_OBJECT_0);
        ::CloseHandle(thread);
    });

    EXPECT_LT(gained, cycles / 4);
}

namespace {

[[noreturn]] void fail_in_the_thread() {
    throw std::runtime_error("проба потока");
}

// A call with no way out for an exception, as a listener is.
void listen_in_the_thread() noexcept {
    fail_in_the_thread();
}

// Nothing is set on the thread of the test: the report comes from the handler the spawned
// thread's entry sets, which MSVC keeps for that thread alone. To stderr rather than to a
// dialog that would hang the run.
void die_in_a_spawned_thread() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);

    thread_scope manager;
    manager->spawn([] { listen_in_the_thread(); });
    manager->join_all();
}

}  // namespace

TEST(ThreadGroupDeathTest, AnExceptionLeavingANoexceptCallOnAThreadIsNamed) {
    EXPECT_DEATH(die_in_a_spawned_thread(), "wxl: unhandled exception: проба потока");
}
