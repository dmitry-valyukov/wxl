#include <gtest/gtest.h>

#include "sync_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/SyncComponent/createDestroy --log_level=test_suite
TEST(SyncComponentTest, createDestroy)
{
    sync_test_component c;

    EXPECT_FALSE(c.on_started_called());
    EXPECT_FALSE(c.on_stopped_called());
}

// --run_test=System/SyncComponent/startAndDestroy --log_level=test_suite
TEST(SyncComponentTest, startAndDestroy)
{
    sync_test_component c;
    c.start_async().get();
    EXPECT_TRUE(c.on_started_called());

    c.stop_async();
}
