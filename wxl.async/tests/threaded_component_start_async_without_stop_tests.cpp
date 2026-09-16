#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/startAsyncWithoutStop --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, startAsyncWithoutStop)
{
    thread_scope manager;
    threaded_test_component component(tracer.name(), manager);

    component.start_async();
    ASSERT_TRUE(true);

    // test should not throw
}
