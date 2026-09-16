#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/simpleStartStop --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, simpleStartStop)
{
    threaded_test_component c(tracer.name());

    c.start_async().get();
    ASSERT_TRUE(c.was_called(component_method::on_started));

    ASSERT_EQ(c.stop_async().wait_for(big_timeout), future_status::ready);
    ASSERT_TRUE(c.was_called(component_method::on_stopped));
}
