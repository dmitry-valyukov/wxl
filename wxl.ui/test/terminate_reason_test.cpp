// What std::terminate says of a winrt::hresult_error: its code, and what the error says of
// itself or, without that, what the system says of the code -- named by the namer wWinMain
// gives wxl.core's handler. The rest of what the handler says is wxl.core's, and tested there.
//
// The handler is the one wWinMain sets, set here by the test itself; the death runs in a
// child process.
#include <crtdbg.h>

#include <gtest/gtest.h>

#include <cstddef>
#include <cstdint>
#include <exception>
#include <stdexcept>
#include <string>
#include <string_view>

// IRestrictedErrorInfo before cppwinrt, as Application.cpp has it: the test asks an error
// whether it has one.
#include "platform.h"

#include <restrictederrorinfo.h>

#include <winrt/base.h>

#include "Application.h"

// The application's namer and the installing of it are its private members; the test reaches
// them through this friend of the class, as the event waits' tests reach theirs.
namespace wxl::impl {

struct application_test {
    static void install_terminate_report() noexcept { Application::install_terminate_report(); }
};

}  // namespace wxl::impl

namespace {

// The handler and its namer, set as wWinMain sets them: once in a process, since the namer is set
// once. A death runs in a process of its own and sets them there.
void set_the_handler_once() {
    static bool const set = (wxl::impl::application_test::install_terminate_report(), true);
    static_cast<void>(set);
}

// The report is read while the exception is the one being handled, as it is in the handler.
template <typename Exception>
std::string describe_thrown(Exception const& exception) {
    set_the_handler_once();
    try {
        throw exception;
    } catch (...) {
        char text[512];
        wxl::core::impl::terminate_report report{text};
        wxl::core::impl::describe_terminate(report);
        return std::string{report.text()};
    }
}

constexpr std::int32_t failed = static_cast<std::int32_t>(0x80004005);  // E_FAIL

// The message of an error of XAML or of the composition is everything the error says; it
// comes as UTF-16 and goes out as UTF-8.
TEST(TerminateReason, names_the_code_and_the_message_of_an_hresult_error) {
    EXPECT_EQ(describe_thrown(winrt::hresult_error(failed, L"нет поверхности")),
              "wxl: unhandled exception: winrt::hresult_error 0x80004005: нет поверхности");
}

// A pair goes out as one code point, a lone half as U+FFFD, and the end of line a message
// may carry does not.
TEST(TerminateReason, writes_the_message_as_utf8) {
    EXPECT_EQ(describe_thrown(winrt::hresult_error(failed,
                                                   L"a\xD83D\xDE00"
                                                   L"b\xD800 \r\n")),
              "wxl: unhandled exception: winrt::hresult_error 0x80004005: "
              "a\xF0\x9F\x98\x80"
              "b\xEF\xBF\xBD");
}

// Without a message of its own, the code is followed by what the error info says of it.
TEST(TerminateReason, names_the_code_of_an_hresult_error) {
    std::string const said = describe_thrown(winrt::hresult_error(failed));
    EXPECT_TRUE(said.starts_with("wxl: unhandled exception: winrt::hresult_error 0x80004005: "))
        << said;
}

// An error with no error info: the code is followed by what the system says of it, in the
// system's language. Made by the default constructor -- E_FAIL, and no info -- because that is
// the one form without any: every constructor with a code originates one, the one that takes
// the thread's info (take_ownership_from_abi) too, when the thread has none of its own to give.
TEST(TerminateReason, names_an_hresult_error_without_error_info) {
    std::string_view const named = "wxl: unhandled exception: winrt::hresult_error 0x80004005: ";
    winrt::hresult_error const bare{};
    ASSERT_FALSE(bare.try_as<::IRestrictedErrorInfo>()) << "the error is to have no info";

    std::string const said = describe_thrown(bare);
    EXPECT_TRUE(said.starts_with(named)) << said;
    EXPECT_GT(said.size(), named.size()) << said;
}

// The code is written as Windows writes one, in capitals.
TEST(TerminateReason, writes_the_code_in_capitals) {
    constexpr std::int32_t out_of_memory = static_cast<std::int32_t>(0x8007000E);  // E_OUTOFMEMORY
    std::string const short_of_memory = describe_thrown(winrt::hresult_error{out_of_memory});
    EXPECT_TRUE(
        short_of_memory.starts_with("wxl: unhandled exception: winrt::hresult_error 0x8007000E: "))
        << short_of_memory;
}

// What is not an hresult_error is left to wxl.core: the namer puts nothing for it.
TEST(TerminateReason, leaves_other_exceptions_to_wxl_core) {
    EXPECT_EQ(describe_thrown(std::runtime_error("no room left")),
              "wxl: unhandled exception: no room left");
    EXPECT_EQ(describe_thrown(42), "wxl: unhandled exception of an unknown type");
}

// The report goes to stderr rather than to a dialog that would hang the run.
void without_a_dialog() { _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT); }

[[noreturn]] void fail() { throw winrt::hresult_error(failed, L"проба terminate"); }

// A call with no way out for an exception, as a listener of the window is.
void listen() noexcept { fail(); }

void throw_out_of_a_noexcept_call() {
    without_a_dialog();
    set_the_handler_once();
    listen();
}

TEST(TerminateReasonDeathTest, names_the_hresult_error_that_left_a_noexcept_call) {
    EXPECT_DEATH(throw_out_of_a_noexcept_call(),
                 "wxl: unhandled exception: winrt::hresult_error 0x80004005: проба terminate");
}

}  // namespace
