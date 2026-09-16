#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/selfDeleteScenario --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, selfDeleteScenario)
{
    thread_scope manager;
    threaded_test_component * component = new threaded_test_component(tracer.name(), manager);
    component->self_delete_ = true;

    component->start_async().get();
    ASSERT_TRUE(component->was_called(component_method::on_started));

    promise<void> is_deleted_promise;
    component->is_deleted_promise_ = is_deleted_promise;

    component->stop_async().get();

    future<void> is_deleted_future = is_deleted_promise.get_future();
    WXL_REQUIRE_BECOME_TRUE([&] { return is_deleted_future.is_ready(); }, big_timeout);
}
