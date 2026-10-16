// What std::terminate says before the process ends: the exception that reached a noexcept
// call -- a listener of the window or of a field -- or that a coroutine nobody holds failed
// with, named in the report core::abort writes, and a terminate with nothing thrown said as
// such.
//
// The handler is the one wWinMain sets, set here by the test itself; the deaths run in a
// child process, which is also where MSVC shows that the exception is still to be had once
// it has left a noexcept call.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

#include <winrt/base.h>

#include "impl/terminate_reason.h"

namespace {

using wxl::impl::describe_terminate;

// The report is read while the exception is the one being handled, as it is in the handler.
template <typename Exception, std::size_t N>
std::string describe_thrown(Exception const& exception, char (&text)[N]) {
    try {
        throw exception;
    } catch (...) {
        return std::string{describe_terminate(text)};
    }
}

constexpr std::int32_t failed = static_cast<std::int32_t>(0x80004005);  // E_FAIL

TEST(TerminateReason, names_what_a_std_exception_says) {
    char text[512];
    EXPECT_EQ(describe_thrown(std::runtime_error("no room left"), text),
              "wxl: unhandled exception: no room left");
}

// An hresult_error is named by its code and by its message, which for an error of XAML or
// the composition is everything the error says; the message comes as UTF-16 and goes out
// as UTF-8.
TEST(TerminateReason, names_the_code_and_the_message_of_an_hresult_error) {
    char text[512];
    EXPECT_EQ(describe_thrown(winrt::hresult_error(failed, L"нет поверхности"), text),
              "wxl: unhandled exception: winrt::hresult_error 0x80004005: нет поверхности");
}

// Without a message of its own, the code is followed by what the system says of it, in the
// system's language.
TEST(TerminateReason, names_the_code_of_an_hresult_error) {
    char text[512];
    std::string const said = describe_thrown(winrt::hresult_error(failed), text);
    EXPECT_TRUE(said.starts_with("wxl: unhandled exception: winrt::hresult_error 0x80004005"))
        << said;
}

TEST(TerminateReason, says_when_the_type_is_unknown) {
    char text[512];
    EXPECT_EQ(describe_thrown(42, text), "wxl: unhandled exception of an unknown type");
}

TEST(TerminateReason, says_when_nothing_was_thrown) {
    char text[512];
    EXPECT_EQ(describe_terminate(text), "wxl: std::terminate without an exception");
}

// A report longer than its buffer is cut where the buffer ends, and the cut is seen.
TEST(TerminateReason, a_report_that_does_not_fit_ends_in_dots) {
    char text[16];
    EXPECT_EQ(describe_thrown(std::runtime_error("longer than sixteen"), text),
              "wxl: unhandle...");
}

// The cut goes back to the start of the code point it would split: the dots follow a whole
// letter, never half of one.
TEST(TerminateReason, a_cut_report_keeps_whole_code_points) {
    char text[32];
    EXPECT_EQ(describe_thrown(std::runtime_error("ЖЖЖЖ"), text),
              "wxl: unhandled exception: Ж...");
}

// The report goes to stderr rather than to a dialog that would hang the run.
void without_a_dialog() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

[[noreturn]] void fail() {
    throw std::runtime_error("проба terminate");
}

// A call with no way out for an exception, as a listener of the window is.
void listen() noexcept {
    fail();
}

void throw_out_of_a_noexcept_call() {
    without_a_dialog();
    wxl::impl::set_terminate_with_reason();
    listen();
}

void terminate_with_nothing_thrown() {
    without_a_dialog();
    wxl::impl::set_terminate_with_reason();
    std::terminate();
}

// What a coroutine nobody holds hands over when it fails: the exception, out of any catch of
// the caller's, named by the handler and by nobody else.
void fail_as_a_detached_coroutine() {
    without_a_dialog();
    wxl::impl::set_terminate_with_reason();
    auto const failure = std::make_exception_ptr(std::runtime_error("корутина упала"));
    wxl::impl::terminate_with_exception(failure);
}

void fail_with_no_exception_to_hand_over() {
    without_a_dialog();
    wxl::impl::set_terminate_with_reason();
    wxl::impl::terminate_with_exception(nullptr);
}

TEST(TerminateReasonDeathTest, names_the_exception_that_left_a_noexcept_call) {
    EXPECT_DEATH(throw_out_of_a_noexcept_call(), "wxl: unhandled exception: проба terminate");
}

TEST(TerminateReasonDeathTest, says_when_nothing_was_thrown) {
    EXPECT_DEATH(terminate_with_nothing_thrown(), "wxl: std::terminate without an exception");
}

TEST(TerminateReasonDeathTest, names_the_exception_a_failed_coroutine_hands_over) {
    EXPECT_DEATH(fail_as_a_detached_coroutine(), "wxl: unhandled exception: корутина упала");
}

TEST(TerminateReasonDeathTest, says_when_a_coroutine_hands_over_nothing) {
    EXPECT_DEATH(fail_with_no_exception_to_hand_over(), "wxl: std::terminate without an exception");
}

}  // namespace
