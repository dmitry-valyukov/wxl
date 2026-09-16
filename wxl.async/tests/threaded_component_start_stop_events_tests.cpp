#include <gtest/gtest.h>


#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/startStopEvents --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, startStopEvents)
{
    thread_scope manager;
    threaded_test_component component(tracer.name(), manager);
    semaphore start_sema(0);
    semaphore stop_sema(0);

    const ticket start_ticket = component.start_async();

    ASSERT_TRUE(!start_sema.try_acquire());

    start_ticket.when_succeeded([&start_sema]() noexcept { start_sema.release(); });
    ASSERT_TRUE(start_sema.try_acquire_for(big_timeout));

    ASSERT_TRUE(component.was_called(component_method::on_started));

    ASSERT_TRUE(!stop_sema.try_acquire());
    component.stop_ticket().when_succeeded([&stop_sema]() noexcept { stop_sema.release(); });
    ASSERT_TRUE(!stop_sema.try_acquire());

    component.stop_async().get();
    ASSERT_TRUE(component.stop_ticket().has_value());
    ASSERT_TRUE(stop_sema.try_acquire_for(big_timeout));
    ASSERT_TRUE(component.was_called(component_method::on_stopped));
}
