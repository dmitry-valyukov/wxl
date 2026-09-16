#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

namespace {

typedef std::shared_ptr<semaphore> semaphore_ptr;
typedef std::vector<semaphore_ptr> semaphores;

void test_thread(countdown_event * event, semaphore_ptr sema)
{
    event->signal();
    sema->acquire();
    event->signal();
}

void launch_one_thread(thread_group * manager, countdown_event * event, semaphores & handles)
{
    semaphore_ptr sema = std::make_shared<semaphore>(0);
    manager->spawn(test_thread, event, sema);
    handles.push_back(sema);
}

void stop_one_thread(semaphores & handles)
{
    semaphore_ptr sema = handles.back();
    handles.pop_back();
    sema->release();
}

}  // namespace

// --run_test=Networking/syncPrimitivesCountdownEventTest --log_level=test_suite
TEST(CountdownEventTest, countdownEvent)
{
#ifdef NDEBUG
    const size_t thread_count = 42;
#else
    const size_t thread_count = 7;
#endif

    const duration small_timeout = duration::from_ms(10);
    const duration big_timeout = duration::from_sec(5);

    countdown_event event(thread_count);

    thread_scope manager;
    {
        semaphores handles;

        for (size_t i = 0; i < thread_count - 1; ++i)
            launch_one_thread(manager, &event, handles);

        EXPECT_FALSE(event.wait_for(small_timeout));

        launch_one_thread(manager, &event, handles);
        EXPECT_TRUE(event.wait_for(big_timeout)); // last

        event.reset(thread_count);

        for (size_t i = 0; i < thread_count - 1; ++i)
            stop_one_thread(handles);

        EXPECT_FALSE(event.wait_for(small_timeout));
        stop_one_thread(handles);
        EXPECT_TRUE(event.wait_for(big_timeout)); // last
    }
}
