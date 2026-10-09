#include <gtest/gtest.h>

#include <crtdbg.h>

import std;
import wxl.core;

namespace {

/// A report goes to stderr and the process ends without a dialog, which in a
/// test run would be a hang.
void without_a_dialog() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

// The default place is the caller's line: the call on the line below.
constexpr std::uint_least32_t line_of_the_call = std::source_location::current().line() + 1;
[[noreturn]] void abort_where_called() { without_a_dialog(); wxl::core::abort("the reason"); }

std::source_location somewhere_else() {
    return std::source_location::current();
}

[[noreturn]] void abort_naming(std::source_location where) {
    without_a_dialog();
    wxl::core::abort("the reason given", where);
}

[[noreturn]] void fail_naming(std::source_location where) {
    without_a_dialog();
    wxl::core::fail("x > 0", where);
}

/// The place as a report spells it: the file's own name, then the line in
/// parentheses -- the form the debugger's output window follows to the source.
std::string place_of(std::uint_least32_t line) {
    return std::format("checks_tests\\.cpp\\({}\\): ", line);
}

}  // namespace

TEST(ChecksDeathTest, AbortNamesTheReasonAndTheLineOfItsCall) {
    EXPECT_DEATH(abort_where_called(), place_of(line_of_the_call) + "the reason");
}

TEST(ChecksDeathTest, AbortNamesThePlaceItIsGiven) {
    const std::source_location where = somewhere_else();

    EXPECT_DEATH(abort_naming(where), place_of(where.line()) + "the reason given");
}

TEST(ChecksDeathTest, AFailedPreconditionIsReportedTheSameWay) {
    const std::source_location where = somewhere_else();

    EXPECT_DEATH(fail_naming(where), place_of(where.line()) + "Precondition x > 0 failed");
}
