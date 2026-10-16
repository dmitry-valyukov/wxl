#include <gtest/gtest.h>

import std;
import wxl.core;

// module_cleanup is process-global and one-shot by design: run_now() fires every
// registered cleanup once, at the end of the process's life, and latches, so a cleanup
// registered afterwards runs at once instead of being deferred. Nothing lowers the latch
// again -- a second life of the mechanism in one process is what it rules out -- so the
// state run_now() leaves is not something a test could tidy away. Each scenario therefore
// runs in a process of its own: EXPECT_EXIT starts the test binary afresh, with nothing
// registered and the latch down, whatever this process ran before and in whatever order.
// The scenario ends that process itself and tells on stderr what the cleanups did.

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

/// Ends the scenario's process the way EXPECT_EXIT in the parent reads it: exit code 0
/// says the scenario got to its end, and one line on stderr says what it saw --
/// "order: 3 2 1." -- so a wrong answer is reported as itself.
[[noreturn]] void exit_telling(std::string_view what, const std::vector<int>& seen) {
    std::cerr << what << ':';
    for (const int value : seen) std::cerr << ' ' << value;
    std::cerr << ".\n";
    std::exit(0);
}

/// Named as gtest names a suite of death tests, which runs ahead of the rest. On Windows
/// every death test re-executes the binary; the threadsafe style asks for the same
/// everywhere, where the fast one would fork this process, latch and registrations
/// included.
class ModuleCleanupDeathTest : public ::testing::Test
{
protected:
    void SetUp() override { GTEST_FLAG_SET(death_test_style, "threadsafe"); }
};

void register_out_of_priority_order_and_run() {
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
    exit_telling("order", order);
}

TEST_F(ModuleCleanupDeathTest, RunsCategoriesFirstThenNormalThenLast) {
    EXPECT_EXIT(register_out_of_priority_order_and_run(), ::testing::ExitedWithCode(0),
                "order: 1 2 3\\.");
}

void register_three_in_one_category_and_run() {
    std::vector<int> order;
    Recorder a{&order, 1};
    Recorder b{&order, 2};
    Recorder c{&order, 3};

    module_cleanup c_a{&record, &a};  // cleanup_normal by default
    module_cleanup c_b{&record, &b};
    module_cleanup c_c{&record, &c};

    module_cleanup::run_now();
    exit_telling("order", order);
}

// LIFO: the last one registered is the first to run.
TEST_F(ModuleCleanupDeathTest, RunsLastRegisteredFirstWithinACategory) {
    EXPECT_EXIT(register_three_in_one_category_and_run(), ::testing::ExitedWithCode(0),
                "order: 3 2 1\\.");
}

void run_twice() {
    int calls = 0;
    module_cleanup c{&increment, &calls};

    module_cleanup::run_now();
    module_cleanup::run_now();  // the latch makes the second call do nothing

    exit_telling("calls", {calls});
}

TEST_F(ModuleCleanupDeathTest, RunNowRunsEachCleanupExactlyOnce) {
    EXPECT_EXIT(run_twice(), ::testing::ExitedWithCode(0), "calls: 1\\.");
}

void register_after_run_now() {
    module_cleanup::run_now();  // latch the mechanism first

    int calls = 0;
    module_cleanup late{&increment, &calls};

    // No further run_now(): registration itself ran it, because the process is
    // already terminating.
    exit_telling("calls", {calls});
}

TEST_F(ModuleCleanupDeathTest, RegisteringAfterRunNowRunsImmediately) {
    EXPECT_EXIT(register_after_run_now(), ::testing::ExitedWithCode(0), "calls: 1\\.");
}

}  // namespace
