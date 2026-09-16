

#include <gtest/gtest.h>




import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

namespace {

void managed_thread_proc(std::atomic<size_t> * enter_counter, std::atomic<size_t> * exit_counter)
{
    enter_counter->fetch_add(1, std::memory_order_relaxed);
    thread::yield();
    exit_counter->fetch_add(1, std::memory_order_relaxed);
}

void detached_thread_proc(semaphore * enter_sema)
{
    thread::yield();

    enter_sema->release();
}

void detached_thread_proc2(semaphore * enter_sema)
{
    enter_sema->acquire();
}

void detached_thread_proc3(std::atomic<size_t> * counter)
{
    counter->fetch_add(1, std::memory_order_relaxed);
}

void broken_thread_proc(thread_group_ptr manager)
{
    try {
        manager->join_all();
        FAIL() << "std::exception should be thrown";
    }
    catch(const std::exception &) {
        // ok
    }
    catch(...) {
        FAIL() << "std::exception should be thrown";
    }
}

}

// --run_test=System/ThreadManagerSuite --log_level=test_suite

// TEST(ThreadGroupTest, np_compile_test) {
//    // We cannot create exemplar of ThreadManager by value
//    // Uncomment the following string to get a compilation error
//
//    // ThreadManager manager;
// }

// --run_test=System/ThreadManagerSuite/simpleCreateDestroyTest --log_level=test_suite
TEST(ThreadGroupTest, simple_create_destroy_test)
{
    thread_group_ptr manager = thread_group::create();

    EXPECT_EQ(1, 1);
}

// --run_test=System/ThreadManagerSuite/simpleCreateDestroy2Test --log_level=test_suite
TEST(ThreadGroupTest, simple_create_destroy2_test)
{
    thread_scope manager;

    EXPECT_EQ(1, 1);
}

// --run_test=System/ThreadManagerSuite/simpleManagedThreadTest --log_level=test_suite
TEST(ThreadGroupTest, simple_managed_thread_test)
{
    std::atomic<size_t> enter_counter{0};
    std::atomic<size_t> exit_counter{0};

    {
        thread_scope manager;
        manager->spawn(managed_thread_proc, &enter_counter, &exit_counter);
    }

    ASSERT_EQ(1u, enter_counter.load());
    ASSERT_EQ(1u, exit_counter.load());
}

// --run_test=System/ThreadManagerSuite/simpleDetachedThreadTest  --log_level=test_suite
TEST(ThreadGroupTest, simple_detached_thread_test)
{
    const duration big_enough_timeout = duration::from_ms(5000);
    const duration small_timeout = duration::from_ms(10);

    semaphore test_sem(0);

    thread_group::spawn_detached(detached_thread_proc, &test_sem);

    ASSERT_EQ(true, test_sem.try_acquire_for(big_enough_timeout));
    ASSERT_EQ(true, thread_group::join_detached_threads_for(big_enough_timeout));

    thread_group::spawn_detached(detached_thread_proc2, &test_sem);

    ASSERT_TRUE(!thread_group::join_detached_threads_for(small_timeout));
    test_sem.release();
    ASSERT_TRUE(thread_group::join_detached_threads_for(big_enough_timeout));
}

// --run_test=System/ThreadManagerSuite/simpleDetachedThread2Test --log_level=test_suite
TEST(ThreadGroupTest, simple_detached_thread2_test)
{
    semaphore thread_enter_sem(0);

    thread_group::spawn_detached(detached_thread_proc, &thread_enter_sem);

    const duration big_enough_timeout = duration::from_ms(5000);

    ASSERT_EQ(true, thread_enter_sem.try_acquire_for(big_enough_timeout));
    ASSERT_EQ(true, thread_group::join_detached_threads_for(big_enough_timeout));
}

// --run_test=System/ThreadManagerSuite/managedThreadStressTest --log_level=test_suite
TEST(ThreadGroupTest, managed_thread_stress_test)
{
#ifdef NDEBUG
    const int max_thread_count = 1024;
#else
    const int max_thread_count = 64;
#endif

    size_t spawned_threads = 0;
    std::atomic<size_t> enter_counter{0};
    std::atomic<size_t> exit_counter{0};

    std::function<void()> thread_proc = [&enter_counter, &exit_counter] { managed_thread_proc(&enter_counter, &exit_counter); };

    {
        thread_scope manager;

        for (int i = 0; i < max_thread_count; i++) {
            manager->spawn(thread_proc);
            spawned_threads++;
            thread::yield();
        }
    }

    ASSERT_EQ(spawned_threads, enter_counter.load());
    ASSERT_EQ(spawned_threads, exit_counter.load());
}

// --run_test=System/ThreadManagerSuite/detachedThreadStressTest --log_level=test_suite
TEST(ThreadGroupTest, detached_thread_stress_test)
{
#ifdef NDEBUG
    const int max_thread_count = 1024;
#else
    const int max_thread_count = 100;
#endif

    std::atomic<size_t> counter{0};

    std::function<void()> thread_proc = [&counter] { detached_thread_proc3(&counter); };

    for (int i = 0; i < max_thread_count; i++) {
        thread_group::spawn_detached(thread_proc);
    }

    // Test that detached threads are finished
    const duration big_enough_timeout = duration::from_ms(5000);
    ASSERT_TRUE(thread_group::join_detached_threads_for(big_enough_timeout));
    ASSERT_EQ(static_cast<size_t>(max_thread_count), counter.load());
}

// --run_test=System/ThreadManagerSuite/closeTest --log_level=test_suite
TEST(ThreadGroupTest, close_test)
{
    // Test that we cannot call to close/joinAll from managed thread spawned from the same ThreadManager exemplar
    thread_group_ptr manager = thread_group::create();
    manager->spawn_named(u8"brokenThreadProc", [manager] { broken_thread_proc(manager); });
    manager->close();

    EXPECT_EQ(1, 1);
}

namespace {

void nop() {
}

void start_detached() {
    thread_group::spawn_detached(nop);
}
}

// --run_test=System/ThreadManagerSuite/detachedThreadStartTest --log_level=test_suite
TEST(ThreadGroupTest, detached_thread_start_test)
{
    const unsigned thread_count = 64;

    for (unsigned i = 0; i < thread_count; i++) {
        thread_group::spawn_detached(start_detached);
    }

    const duration timeout = duration::from_ms(5000);
    EXPECT_TRUE(thread_group::join_detached_threads_for(timeout));
}
