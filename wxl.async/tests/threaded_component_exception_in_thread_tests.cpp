#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/exceptionInThreadScenario --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, exceptionInThreadScenario)
{
    thread_scope manager;
    threaded_test_component component(tracer.name(), manager);
    component.throw_before_method_ = component_method::run;

    component.start_async().get();
    component.stop_ticket().wait();

    ASSERT_TRUE(component.was_called(component_method::on_started));
    ASSERT_TRUE(component.was_called(component_method::on_stopped));
    ASSERT_TRUE(component.stop_reason_ptr());
    ASSERT_TRUE(exception_is<specific_exception>(*component.stop_reason_ptr()));
}
