// The layout facts behind "every string crossing into WinRT is lent as the
// handle it is".
//
// impl::to_winrt() hands a WinRT method a winrt::hstring const& that is nothing
// more than where an hstring_param keeps its handle, reinterpreted. That is
// legal only while three things hold, and each is checked here where both
// types meet: winrt::hstring and core::hstring are one pointer wide, the
// handle an hstring_param keeps is the very one it was lent -- an hstring's own,
// or a fast-pass header over the caller's characters -- and cppwinrt reads that
// header as a string of its own.
//
// What hstring_param takes and refuses is proven in wxl.core's tests; this file
// is the half that needs cppwinrt.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include <winrt/base.h>

#include "hstring_param.h"

namespace {

// The borrow to_winrt() makes, spelled as it is spelled there.
winrt::hstring const& lend(wxl::hstring_param const& param) {
    return *reinterpret_cast<winrt::hstring const*>(&param.as_hstring());
}

static_assert(sizeof(wxl::hstring) == sizeof(winrt::hstring));
static_assert(alignof(wxl::hstring) == alignof(winrt::hstring));
static_assert(std::is_standard_layout_v<wxl::hstring>);

TEST(hstring_param, an_hstring_is_lent_as_the_handle_it_is) {
    wxl::hstring const text{u"Буквица"};
    wxl::hstring_param const param{text};

    EXPECT_EQ(winrt::get_abi(lend(param)), text.get_abi());
    EXPECT_EQ(std::wstring_view{lend(param)}, std::wstring_view{L"Буквица"});
}

TEST(hstring_param, text_is_lent_by_a_header_over_the_callers_characters) {
    std::u16string const owner = u"Буквица";
    wxl::hstring_param const param{owner};

    // No copy: cppwinrt sees the characters where the owner keeps them.
    winrt::hstring const& value = lend(param);
    EXPECT_EQ(static_cast<void const*>(value.c_str()), static_cast<void const*>(owner.c_str()));
    EXPECT_EQ(value.size(), owner.size());
}

// Above the BMP, where a code point is two units: the count is what makes the
// unit real, and it has to survive the crossing.
TEST(hstring_param, a_surrogate_pair_stays_two_units) {
    wxl::hstring_param const param{u"\U0001F4D6"};

    EXPECT_EQ(lend(param).size(), 2u);
}

TEST(hstring_param, a_borrowed_string_that_is_kept_is_duplicated_for_real) {
    std::u16string const owner = u"Буквица";
    wxl::hstring_param const param{owner};

    // What a property does with a string it keeps: cppwinrt duplicates the
    // fast-pass string, which is what makes a real one, on a heap of its own.
    winrt::hstring const kept = lend(param);
    EXPECT_NE(winrt::get_abi(kept), winrt::get_abi(lend(param)));
    EXPECT_NE(static_cast<void const*>(kept.c_str()), static_cast<void const*>(owner.c_str()));
    EXPECT_EQ(std::wstring_view{kept}, std::wstring_view{L"Буквица"});
}

TEST(hstring_param, empty_is_the_null_handle_both_ways) {
    wxl::hstring_param const none;
    wxl::hstring_param const empty{u""};

    EXPECT_TRUE(lend(none).empty());
    EXPECT_TRUE(lend(empty).empty());
    EXPECT_EQ(winrt::get_abi(lend(empty)), nullptr);
}

// Crossing back: a string read off WinRT keeps the very HSTRING, one reference
// more, and reads the same text.
TEST(hstring_param, a_string_read_from_winrt_shares_the_handle) {
    winrt::hstring const from_winrt{L"Буквица"};

    wxl::hstring result;
    winrt::copy_to_abi(from_winrt, *result.put_abi());

    EXPECT_EQ(result.get_abi(), winrt::get_abi(from_winrt));
    EXPECT_TRUE(result == u"Буквица");
}

}  // namespace
