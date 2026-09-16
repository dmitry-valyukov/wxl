#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/startStopRequest--log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, startStopRequest)
{
    thread_scope manager;
    threaded_test_component component(tracer.name(), manager);

    component.start_async().get();

    component.stop_async(stop_reason::abort());
    ASSERT_EQ(component.stop_request_count_.load(), 1u);

    component.stop_async();
    // check the same number of effective stop requests
    ASSERT_EQ(component.stop_request_count_.load(), 1u);

    ASSERT_EQ(component.stop_ticket().wait_for(big_timeout), future_status::ready);

    // check the stop reason should be the same as the reason of the first stop_async() call
    ASSERT_TRUE(component.stop_reason_ptr());
    ASSERT_EQ(*component.stop_reason_ptr(), stop_reason::abort());
}
