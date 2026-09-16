// The proof behind "every string crossing into WinRT goes through
// impl::to_winrt". winrt::param::hstring legitimately accepts a
// std::wstring_view -- but its contract is a zero right after the viewed
// range: cppwinrt's create_hstring_on_stack (winrt/base.h) checks
// value[length] and calls abort() outright, no exception, no HRESULT.
// A literal, a std::wstring or a pooled sta_wstring satisfy that for free;
// a view into a parser's arena does not -- which is exactly what took the
// HtmlView sample down before FormattedBlock.cpp switched to to_winrt.
//
// Three facts, one test each: the terminated view is legal, the
// unterminated one dies, and the real winrt::hstring copy (to_winrt's
// path) needs no terminator at all.

#include <gtest/gtest.h>

#include <cstdlib>
#include <string>
#include <string_view>

#include <winrt/base.h>

namespace {

// A buffer where the character after the viewed range is deliberately not
// zero -- the shape of a view into an arena, where the next allocation's
// bytes sit right behind.
constexpr wchar_t unterminated[] = {L'a', L'b', L'c', L'!', L'x'};

TEST(param_hstring, terminated_view_is_legal) {
    const std::wstring owned = L"terminated";
    const winrt::param::hstring param{std::wstring_view{owned}};
    const winrt::hstring& value = param;
    EXPECT_EQ(std::wstring_view{value}, std::wstring_view{owned});
}

// A function, not a statement block: the commas inside would split
// EXPECT_DEATH's macro arguments. The debug CRT would put up its abort()
// dialog and hang the death-test child; report the death by exiting
// instead.
void construct_from_unterminated() {
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    const winrt::param::hstring param{std::wstring_view{unterminated, 3}};
    (void)param;
}

TEST(param_hstring, unterminated_view_aborts) {
    EXPECT_DEATH(construct_from_unterminated(), "");
}

TEST(param_hstring, real_hstring_copies_and_needs_no_terminator) {
    const winrt::hstring copy{std::wstring_view{unterminated, 3}};
    EXPECT_EQ(std::wstring_view{copy}, (std::wstring_view{unterminated, 3}));
}

}  // namespace
