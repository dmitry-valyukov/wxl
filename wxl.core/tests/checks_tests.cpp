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

TEST(ChecksTest, AReportWritesTheLineOfAnAbortAndGoesOn) {
    testing::internal::CaptureStderr();
    constexpr std::uint_least32_t line_of_the_report = std::source_location::current().line() + 1;
    wxl::core::report("the note");
    const std::string said = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(std::regex_search(said, std::regex(place_of(line_of_the_report) + "the note\\n$"))) << said;
}

TEST(ChecksTest, AReportNamesThePlaceItIsGiven) {
    const std::source_location where = somewhere_else();

    testing::internal::CaptureStderr();
    wxl::core::report("the note given", where);
    const std::string said = testing::internal::GetCapturedStderr();

    EXPECT_TRUE(std::regex_search(said, std::regex(place_of(where.line()) + "the note given\\n$"))) << said;
}

TEST(ChecksDeathTest, AFailedPreconditionIsReportedTheSameWay) {
    const std::source_location where = somewhere_else();

    EXPECT_DEATH(fail_naming(where), place_of(where.line()) + "Precondition x > 0 failed");
}

// ---- What std::terminate says ------------------------------------------------------------

namespace {

using wxl::core::impl::describe_terminate;
using wxl::core::impl::terminate_report;

// The report is read while the exception is the one being handled, as it is in the handler.
template <typename Exception, std::size_t N>
std::string describe_thrown(const Exception& exception, char (&text)[N]) {
    try {
        throw exception;
    } catch (...) {
        terminate_report report{text};
        describe_terminate(report);
        return std::string{report.text()};
    }
}

/// An exception of a type wxl.core does not know, as the projection's error is one.
struct foreign_error {
    int code;
};

bool name_foreign_error(terminate_report& report) noexcept {
    try {
        throw;
    } catch (const foreign_error& error) {
        report.put("wxl: unhandled exception: foreign_error ");
        report.put(std::to_string(error.code));
        return true;
    } catch (...) {
        return false;
    }
}

}  // namespace

TEST(TerminateReason, NamesWhatAStdExceptionSays) {
    char text[512];
    EXPECT_EQ(describe_thrown(std::runtime_error("no room left"), text),
              "wxl: unhandled exception: no room left");
}

// No namer is set in this process: the namer's own probes run in processes of their own.
TEST(TerminateReason, SaysWhenTheTypeIsUnknown) {
    char text[512];
    EXPECT_EQ(describe_thrown(42, text), "wxl: unhandled exception of an unknown type");
    EXPECT_EQ(describe_thrown(foreign_error{7}, text), "wxl: unhandled exception of an unknown type");
}

TEST(TerminateReason, SaysWhenNothingWasThrown) {
    char text[512];
    terminate_report report{text};
    describe_terminate(report);
    EXPECT_EQ(report.text(), "wxl: std::terminate without an exception");
}

namespace {

// The namer is set once in a process, so a probe that sets it runs in a child of its own:
// EXPECT_EXIT starts the program again, the child sets the namer, writes what three
// reports said as one line to stderr and exits with 0, so a wrong answer is seen as is.
void describe_with_the_namer() {
    wxl::core::impl::set_exception_namer(&name_foreign_error);

    char text[512];
    std::cerr << "said: " << describe_thrown(foreign_error{7}, text) << " / "
              << describe_thrown(42, text) << " / " << describe_thrown(std::logic_error("known"), text)
              << " end\n";
    std::exit(0);
}

void set_a_second_namer() {
    without_a_dialog();
    wxl::core::impl::set_exception_namer(&name_foreign_error);
    wxl::core::impl::set_exception_namer(&name_foreign_error);
}

}  // namespace

// The layer that knows a type names it; what its namer does not know is left to the names
// wxl.core has, and a std::exception never reaches the namer.
TEST(TerminateReasonDeathTest, AsksTheNamerAboutATypeItDoesNotKnow) {
    EXPECT_EXIT(describe_with_the_namer(), ::testing::ExitedWithCode(0),
                "said: wxl: unhandled exception: foreign_error 7 / "
                "wxl: unhandled exception of an unknown type / "
                "wxl: unhandled exception: known end");
}

// One namer for the process: a second, while one is set, is a broken rule.
TEST(TerminateReasonDeathTest, ASecondNamerIsABrokenRule) {
    EXPECT_DEATH(set_a_second_namer(), "wxl: an exception namer is set already");
}

// A report longer than its buffer is cut where the buffer ends, and the cut is seen.
TEST(TerminateReason, AReportThatDoesNotFitEndsInDots) {
    char text[16];
    EXPECT_EQ(describe_thrown(std::runtime_error("longer than sixteen"), text), "wxl: unhandle...");
}

// The cut goes back to the start of the code point it would split: the dots follow a whole
// letter, never half of one.
TEST(TerminateReason, ACutReportKeepsWholeCodePoints) {
    char text[32];
    EXPECT_EQ(describe_thrown(std::runtime_error("ЖЖЖЖ"), text), "wxl: unhandled exception: Ж...");
}

// UTF-16 goes in as UTF-8, a pair as one code point and a lone half as U+FFFD; and a cut
// inside a four-byte sequence takes the whole of it.
TEST(TerminateReason, PutsUtf16AsUtf8) {
    char text[16];
    terminate_report report{text};
    report.put(L"a\xD83D\xDE00" L"b\xD800" L"c");
    EXPECT_EQ(report.text(), "a\xF0\x9F\x98\x80" "b\xEF\xBF\xBD" "c");

    char short_text[8];
    terminate_report cut{short_text};
    cut.put(L"ab\xD83D\xDE00\xD83D\xDE00");
    EXPECT_EQ(cut.text(), "ab...");
}

// The report is what it says each time it is asked, and a cut one takes nothing more: a
// part that would fit after the dots is not the part that was lost.
TEST(TerminateReason, ACutReportStaysAsItWas) {
    char text[8];
    terminate_report report{text};
    report.put("abcdefghij");
    EXPECT_EQ(report.text(), "abcde...");
    EXPECT_EQ(report.text(), "abcde...");

    report.put("z");
    report.put(L"z");
    EXPECT_EQ(report.text(), "abcde...");

    // A cut that went back to the start of a code point leaves room after the dots.
    char wide_text[8];
    terminate_report wide{wide_text};
    wide.put(L"ab\xD83D\xDE00\xD83D\xDE00");
    wide.put("z");
    wide.put(L"z");
    EXPECT_EQ(wide.text(), "ab...");
}

// The smallest report there is: the dots alone, or what fits whole.
TEST(TerminateReason, ThreeBytesHoldTheDots) {
    char text[3];
    terminate_report fits{text};
    fits.put("ab");
    EXPECT_EQ(fits.text(), "ab");

    terminate_report cut{text};
    cut.put("abcd");
    EXPECT_EQ(cut.text(), "...");
}

namespace {

[[noreturn]] void fail_with_a_runtime_error() {
    throw std::runtime_error("проба terminate");
}

// A call with no way out for an exception, as a listener of the window is.
void listen() noexcept {
    fail_with_a_runtime_error();
}

[[noreturn]] void fail_with_a_foreign_error() {
    throw foreign_error{7};
}

void listen_with_a_foreign_error() noexcept {
    fail_with_a_foreign_error();
}

void throw_out_of_a_noexcept_call() {
    without_a_dialog();
    static_cast<void>(wxl::core::impl::set_terminate_with_reason());
    listen();
}

void terminate_with_nothing_thrown() {
    without_a_dialog();
    static_cast<void>(wxl::core::impl::set_terminate_with_reason());
    std::terminate();
}

// What a coroutine nobody holds hands over when it fails: the exception, out of any catch of
// the caller's, named by the handler and by nobody else. Nothing sets the handler on this
// thread beforehand: terminate_with_exception sets it for the thread it fails on.
void fail_as_a_detached_coroutine() {
    without_a_dialog();
    const auto failure = std::make_exception_ptr(std::runtime_error("корутина упала"));
    wxl::core::impl::terminate_with_exception(failure);
}

void fail_with_no_exception_to_hand_over() {
    without_a_dialog();
    wxl::core::impl::terminate_with_exception(nullptr);
}

[[noreturn]] void fail_while_naming() {
    throw std::runtime_error("the namer failed");
}

// A namer that throws: the exception leaves a noexcept call inside the handler.
bool name_by_throwing(terminate_report&) noexcept {
    fail_while_naming();
}

// Describing the exception brings std::terminate back; the handler entered again stops with
// a line of its own instead of describing it again, and again.
void throw_from_inside_the_report() {
    without_a_dialog();
    wxl::core::impl::set_exception_namer(&name_by_throwing);
    static_cast<void>(wxl::core::impl::set_terminate_with_reason());
    listen_with_a_foreign_error();
}

}  // namespace

// The exception is named after it has left the noexcept call: the handler rethrows the one
// being handled, which is the object thrown, not a copy.
TEST(TerminateReasonDeathTest, NamesTheExceptionThatLeftANoexceptCall) {
    EXPECT_DEATH(throw_out_of_a_noexcept_call(), "wxl: unhandled exception: проба terminate");
}

TEST(TerminateReasonDeathTest, SaysWhenNothingWasThrown) {
    EXPECT_DEATH(terminate_with_nothing_thrown(), "wxl: std::terminate without an exception");
}

TEST(TerminateReasonDeathTest, NamesTheExceptionAFailedCoroutineHandsOver) {
    EXPECT_DEATH(fail_as_a_detached_coroutine(), "wxl: unhandled exception: корутина упала");
}

TEST(TerminateReasonDeathTest, SaysWhenACoroutineHandsOverNothing) {
    EXPECT_DEATH(fail_with_no_exception_to_hand_over(), "wxl: std::terminate without an exception");
}

TEST(TerminateReasonDeathTest, ATerminateWhileDescribingStops) {
    EXPECT_DEATH(throw_from_inside_the_report(),
                 "wxl: std::terminate again while its exception was being described");
}
