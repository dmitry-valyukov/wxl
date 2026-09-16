#include <gtest/gtest.h>

#include "threaded_test_component.h"

import std;
import wxl.core;
import wxl.async;

using namespace wxl::core;
using namespace wxl::async;

// --run_test=System/ThreadedComponent/sharedResources --log_level=test_suite
STRESS_TEST_CASE(ThreadedComponent, sharedResources)
{
    thread_scope manager;

    threaded_test_component parent1("SyncRoot:parent1", manager);
    threaded_test_component child1("SyncRoot:parent1:child1", manager, parent1.sync_root());
    threaded_test_component child11("SyncRoot:parent1:child11", manager, child1.sync_root());

    ASSERT_TRUE(parent1.sync_root() == child1.sync_root());
    ASSERT_TRUE(child1.sync_root() == child11.sync_root());

    threaded_test_component parent2("SyncRoot:parent2", manager);
    threaded_test_component child2("SyncRoot:parent2:child2", manager, parent2.sync_root());

    ASSERT_TRUE(parent2.sync_root() == child2.sync_root());
    ASSERT_TRUE(child1.sync_root() != child2.sync_root());
}
