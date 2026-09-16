#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/stopByError --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, stopByError)
{
    threaded_test_component component(tracer.name());
    const ticket start_ticket = component.start_async();
    start_ticket.get();
    const ticket stop_ticket = component.stop_ticket();
    component.stop_async(std::make_exception_ptr(specific_exception()));

    ASSERT_THROW(stop_ticket.get(), specific_exception);
}
