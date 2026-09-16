#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/startStopTicketsUsingPrivateThread --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, startStopTicketsUsingPrivateThread)
{
    thread_scope manager;
    threaded_test_component component(tracer.name(), manager);

    const ticket start_ticket = component.start_async();
    ASSERT_EQ(future_status::ready, start_ticket.wait_for(big_timeout));
    ASSERT_TRUE(start_ticket.has_value());

    component.stop_async();
    ASSERT_EQ(future_status::ready, component.stop_ticket().wait_for(big_timeout));
    ASSERT_TRUE(component.stop_ticket().has_value());
}
