#include <gtest/gtest.h>

#include <string_view>

#include <fmt/compile.h>

import wxl.core;
import wxl.fmt;

using namespace std::string_view_literals;
using wxl::core::u16_text;
using wxl::core::u16_view;

namespace {

TEST(format, numbers_and_checked_text) {
    const u16_text text = wxl::core::format(u"{} {} {}", 7, u16_view{u"из"}, 3.5);

    EXPECT_EQ(text.plain(), u"7 из 3.5"sv);
}

TEST(format, a_precision_asked_for) {
    EXPECT_EQ(wxl::core::format(u"{:.9g}", 1.0 / 3).plain(), u"0.333333333"sv);
    EXPECT_EQ(wxl::core::format(u"{:.9g}", 64.0).plain(), u"64"sv);
    EXPECT_EQ(wxl::core::format(u"{:.9g}", -1e-5).plain(), u"-1e-05"sv);
}

TEST(format, text_it_was_handed) {
    const u16_text owned{u16_view{u"—"}};

    EXPECT_EQ(wxl::core::format(u"[{}]", owned).plain(), u"[—]"sv);
    EXPECT_EQ(wxl::core::format(u"[{}]", u16_view{}).plain(), u"[]"sv);
}

TEST(format, a_compiled_format_string) {
    EXPECT_EQ(wxl::core::format(FMT_COMPILE(u"{}x{}"), 3, 4).plain(), u"3x4"sv);
}

TEST(format, a_result_longer_than_the_buffer_inside) {
    const u16_text text = wxl::core::format(u"{:>300}", 1);

    EXPECT_EQ(text.size(), 300u);
    EXPECT_EQ(text.plain().back(), u'1');
}

// Only what is known to be well-formed goes in: raw text and single units do
// not compile, whatever they happen to hold.
template <typename T>
constexpr bool formats = requires(const T& value) { wxl::core::format(u"{}", value); };

static_assert(formats<int>);
static_assert(formats<double>);
static_assert(formats<u16_view>);
static_assert(formats<u16_text>);
static_assert(!formats<std::u16string_view>);
static_assert(!formats<const char16_t*>);
static_assert(!formats<char16_t>);
static_assert(!formats<std::string_view>);
static_assert(!formats<wxl::core::u8_view>);

}  // namespace
