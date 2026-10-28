// The STA thread is the thread the program's static initialization ran on, the one a test's
// body runs on as well: there sta_thread says it is current, on any other thread it says it is
// not, and ensure_current() stops the process from another thread, naming the caller's line, in
// every build.
#include <crtdbg.h>

#include <gtest/gtest.h>

import std;
import wxl.core;

using namespace wxl::async;

namespace {

/// What sta_thread says on a thread of its own.
bool is_current_on_another_thread() {
    bool current = true;
    std::thread([&current] { current = sta_thread::is_current(); }).join();
    return current;
}

/// A failed check goes to stderr and ends the process without a dialog, so
/// that a death test can read why.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

// clang-format off: the test reads the line of the call as its own line + 1
constexpr std::uint_least32_t line_of_the_foreign_call = std::source_location::current().line() + 1;
void ensure_current_there() { sta_thread::ensure_current(); }
// clang-format on

void ensure_current_on_another_thread() {
    report_failures_to_stderr();

    std::thread([] { ensure_current_there(); }).join();
}

}  // namespace

TEST(StaThreadTest, TheTestsThreadIsTheStaThread) { EXPECT_TRUE(sta_thread::is_current()); }

TEST(StaThreadTest, AnotherThreadIsNotTheStaThread) {
    EXPECT_FALSE(is_current_on_another_thread());
}

TEST(StaThreadTest, EnsureCurrentLetsTheStaThreadThrough) { sta_thread::ensure_current(); }

TEST(StaThreadDeathTest, EnsureCurrentStopsAnotherThreadAtTheCallersLine) {
    EXPECT_DEATH(ensure_current_on_another_thread(),
                 std::format("sta_thread_tests\\.cpp\\({}\\): called from a thread other than "
                             "the STA thread",
                             line_of_the_foreign_call));
}
