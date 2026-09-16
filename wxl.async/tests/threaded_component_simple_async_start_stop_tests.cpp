#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/simpleAsyncStartStop --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, simpleAsyncStartStop)
{
    threaded_test_component component(tracer.name());
    const ticket start_ticket = component.start_async();

    ASSERT_TRUE(start_ticket.wait_for(big_timeout) == future_status::ready);
    ASSERT_TRUE(component.was_called(component_method::on_started));

    component.stop_async();
    ASSERT_TRUE(component.stop_ticket().wait_for(big_timeout) == future_status::ready);

    component.stop_ticket().get(); // should not throw
    ASSERT_TRUE(component.was_called(component_method::on_stopped));
}
