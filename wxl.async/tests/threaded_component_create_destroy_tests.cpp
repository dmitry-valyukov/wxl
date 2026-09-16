#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/createDestroy  --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, createDestroy)
{
    promise<void> delete_promise;
    future<void> delete_fut = delete_promise.get_future();

    {
        threaded_test_component component(tracer.name());
        component.is_deleted_promise_ = delete_promise;

        ASSERT_TRUE(!component.stop_requested());
        ASSERT_TRUE(!delete_fut.is_ready());
    }

    ASSERT_TRUE(delete_fut.is_ready());
}
