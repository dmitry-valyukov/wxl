// The seam between the two spellings of UTF-16, proven rather than asserted
// in prose.
//
// wxl is moving its text to char16_t, because that is the unit the standard
// fixes at sixteen bits and the unit an HSTRING is made of, while wchar_t is
// sixteen bits on MSVC and thirty-two on gcc and clang. string_param is what
// lets the move be gradual: the declarative syntax is written in L"..."
// everywhere and keeps working, while new code writes u"...".
//
// What has to hold is that crossing between the two is a renaming and not a
// conversion -- same units, same count, nothing copied.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

#include "string_param.h"

namespace {

TEST(string_param, wide_literal_reads_as_char16) {
    wxl::string_param const text = L"Буквица";
    EXPECT_EQ(text.text(), std::u16string_view{u"Буквица"});
}

TEST(string_param, char16_literal_is_taken_as_it_is) {
    wxl::string_param const text = u"Буквица";
    EXPECT_EQ(text.text(), std::u16string_view{u"Буквица"});
}

// Above the BMP, where a code point is two units. The count is what makes
// the difference between the two types real: on a platform whose wchar_t is
// thirty-two bits this text would be one wchar_t and is always two char16_t,
// which is why the unit is named rather than inherited from the platform.
TEST(string_param, surrogate_pair_stays_two_units) {
    wxl::string_param const text = L"\U0001F4D6";
    EXPECT_EQ(text.text().size(), 2u);
    EXPECT_EQ(text.text(), std::u16string_view{u"\U0001F4D6"});
}

// A string of either unit arrives as a view of itself: the parameter points
// into the caller's buffer, and no copy is made on the way in.
TEST(string_param, a_string_is_viewed_not_copied) {
    std::wstring const wide = L"one two";
    std::u16string const units = u"one two";

    EXPECT_EQ(static_cast<void const*>(wxl::string_param{wide}.text().data()),
              static_cast<void const*>(wide.data()));
    EXPECT_EQ(wxl::string_param{units}.text().data(), units.data());
}

TEST(string_param, empty_is_empty_whichever_way_it_is_written) {
    EXPECT_TRUE(wxl::string_param{L""}.empty());
    EXPECT_TRUE(wxl::string_param{u""}.empty());
    EXPECT_TRUE(wxl::string_param{std::u16string{}}.empty());
}

}  // namespace
