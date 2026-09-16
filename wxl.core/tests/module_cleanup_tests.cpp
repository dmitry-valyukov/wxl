#include <gtest/gtest.h>

import std;
import wxl.core;

// module_cleanup is process-global and one-shot: the first time it winds down
// -- here driven explicitly by run_now() -- it fires every registered cleanup
// once and latches, so any registration afterwards runs immediately instead of
// being deferred. That is why each test below lives in its own process (which
// gtest_discover_tests gives): once one has called run_now(), no later
// registration in the same process could observe deferral again.

namespace {

using wxl::core::module_cleanup;

// The void(void*) registration form carries its output without a capture: each
// cleanup is handed a Recorder, and appends its id to the shared log.
struct Recorder {
    std::vector<int>* log;
    int id;
};

void record(Recorder* r) { r->log->push_back(r->id); }

void increment(int* n) { ++*n; }

TEST(ModuleCleanupTest, RunsCategoriesFirstThenNormalThenLast) {
    std::vector<int> order;
    Recorder first{&order, 1};
    Recorder normal{&order, 2};
    Recorder last{&order, 3};

    // Registered out of priority order on purpose: the run order must follow
    // the category, not the moment of registration.
    module_cleanup c_last{&record, &last, module_cleanup::cleanup_last};
    module_cleanup c_first{&record, &first, module_cleanup::cleanup_first};
    module_cleanup c_normal{&record, &normal, module_cleanup::cleanup_normal};

    module_cleanup::run_now();

    EXPECT_EQ(order, (std::vector<int>{1, 2, 3}));
}

TEST(ModuleCleanupTest, RunsLastRegisteredFirstWithinACategory) {
    std::vector<int> order;
    Recorder a{&order, 1};
    Recorder b{&order, 2};
    Recorder c{&order, 3};

    module_cleanup c_a{&record, &a};  // cleanup_normal by default
    module_cleanup c_b{&record, &b};
    module_cleanup c_c{&record, &c};

    module_cleanup::run_now();

    // LIFO: the last one registered is the first to run.
    EXPECT_EQ(order, (std::vector<int>{3, 2, 1}));
}

TEST(ModuleCleanupTest, RunNowRunsEachCleanupExactlyOnce) {
    int calls = 0;
    module_cleanup c{&increment, &calls};

    module_cleanup::run_now();
    module_cleanup::run_now();  // the latch makes the second call do nothing

    EXPECT_EQ(calls, 1);
}

TEST(ModuleCleanupTest, RegisteringAfterRunNowRunsImmediately) {
    module_cleanup::run_now();  // latch the mechanism first

    int calls = 0;
    module_cleanup late{&increment, &calls};

    // No further run_now(): registration itself ran it, because the process is
    // already terminating.
    EXPECT_EQ(calls, 1);
}

}  // namespace
