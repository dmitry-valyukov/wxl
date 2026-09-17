#include <gtest/gtest.h>

#include <cstdint>
#include <locale>
#include <optional>
#include <string>
#include <string_view>

import wxl.core;

using namespace std::string_view_literals;

namespace {

/// Sets a locale whose decimal separator is a comma for as long as it lives.
/// The point of this partition is that nothing it does changes when this is in
/// force, and the only way to show that is to put it in force.
class comma_locale {
public:
    comma_locale() {
        try {
            was_ = std::locale::global(std::locale("ru-RU"));
            applied_ = true;
        } catch (const std::runtime_error&) {
            // A machine without the Russian locale installed cannot run this
            // half of the test; the assertions below still check the values.
        }
    }

    ~comma_locale() {
        if (applied_) std::locale::global(was_);
    }

    bool applied() const { return applied_; }

private:
    std::locale was_;
    bool applied_ = false;
};

// try_parse writes the result only for a whole number, and leaves it alone
// otherwise -- which is what lets a caller with a default write the default
// first and call once. No value of the type is spent on "nothing", so the
// whole range comes through: the smallest int, the largest byte.
TEST(numbers, try_parse_fills_the_result_only_for_a_whole_number) {
    int value = 7;
    EXPECT_TRUE(wxl::core::try_parse("42"sv, value));
    EXPECT_EQ(value, 42);
    EXPECT_TRUE(wxl::core::try_parse("-42"sv, value));
    EXPECT_EQ(value, -42);

    EXPECT_FALSE(wxl::core::try_parse("42px"sv, value));  // half a number is not one
    EXPECT_EQ(value, -42);                                   // and the result is untouched
    EXPECT_FALSE(wxl::core::try_parse(" 42"sv, value));
    EXPECT_FALSE(wxl::core::try_parse(""sv, value));
    EXPECT_FALSE(wxl::core::try_parse("x"sv, value));
    EXPECT_EQ(value, -42);

    EXPECT_TRUE(wxl::core::try_parse("-2147483648"sv, value));
    EXPECT_EQ(value, -2147483647 - 1);

    std::uint8_t byte = 0;
    EXPECT_TRUE(wxl::core::try_parse("255"sv, byte));
    EXPECT_EQ(byte, 255);
    EXPECT_FALSE(wxl::core::try_parse("300"sv, byte));  // does not fit
    EXPECT_EQ(byte, 255);

    std::uint64_t big = 0;
    EXPECT_TRUE(wxl::core::try_parse("18446744073709551615"sv, big));
    EXPECT_EQ(big, 18446744073709551615ull);
}

TEST(numbers, try_parse_a_point_is_a_point) {
    const comma_locale comma;

    double real = 0;
    EXPECT_TRUE(wxl::core::try_parse("1.5"sv, real));
    EXPECT_EQ(real, 1.5);
    EXPECT_FALSE(wxl::core::try_parse("1,5"sv, real));
    EXPECT_EQ(real, 1.5);
    EXPECT_TRUE(wxl::core::try_parse("-0.125"sv, real));
    EXPECT_EQ(real, -0.125);

    float single = 0;
    EXPECT_TRUE(wxl::core::try_parse("2.5"sv, single));
    EXPECT_EQ(single, 2.5f);
}

TEST(numbers, try_parse_reads_utf16_text_the_same_way) {
    const comma_locale comma;

    int value = 0;
    EXPECT_TRUE(wxl::core::try_parse(L"42"sv, value));
    EXPECT_EQ(value, 42);
    EXPECT_FALSE(wxl::core::try_parse(L"４２"sv, value));  // full-width digits are not
    EXPECT_EQ(value, 42);

    // The allocator is named as a template, and T is still deduced.
    EXPECT_TRUE(wxl::core::try_parse<std::allocator>(L"7"sv, value));
    EXPECT_EQ(value, 7);

    double real = 0.5;
    EXPECT_FALSE(wxl::core::try_parse(L"1,5"sv, real));
    EXPECT_EQ(real, 0.5);
    EXPECT_TRUE(wxl::core::try_parse(L"1.5"sv, real));
    EXPECT_EQ(real, 1.5);

    // Longer than the stack half of the buffer: still the same answer.
    std::wstring long_number = L"0.";
    long_number.append(400, L'1');
    EXPECT_TRUE(wxl::core::try_parse(std::wstring_view{long_number}, real));
}

TEST(numbers, parsing_a_number_with_something_after_it) {
    const auto length = wxl::core::parse_prefix<float>("20mm"sv);

    ASSERT_TRUE(length.has_value());
    EXPECT_EQ(length->value, 20.0f);
    EXPECT_EQ(length->rest, "mm");

    const auto share = wxl::core::parse_prefix<int>("50%"sv);
    ASSERT_TRUE(share.has_value());
    EXPECT_EQ(share->value, 50);
    EXPECT_EQ(share->rest, "%");

    const auto whole = wxl::core::parse_prefix<int>("12"sv);
    ASSERT_TRUE(whole.has_value());
    EXPECT_TRUE(whole->rest.empty());

    EXPECT_FALSE(wxl::core::parse_prefix<int>("auto"sv).has_value());
}

TEST(numbers, printing_a_point_is_a_point) {
    const comma_locale comma;

    EXPECT_EQ(wxl::core::to_string(1.5), "1.5");
    EXPECT_EQ(wxl::core::to_string(-0.125), "-0.125");
    EXPECT_EQ(wxl::core::to_string(42), "42");
    EXPECT_EQ(wxl::core::to_wstring(1.5), L"1.5");
    EXPECT_EQ(wxl::core::to_wstring(-7), L"-7");
    EXPECT_EQ(wxl::core::to_u16(-2.5), u"-2.5");
    EXPECT_EQ(wxl::core::to_u16(64), u"64");
}

TEST(numbers, printing_in_the_format_asked_for) {
    const comma_locale comma;

    EXPECT_EQ(wxl::core::to_string(2.0 / 3.0, std::chars_format::fixed, 2), "0.67");
    EXPECT_EQ(wxl::core::to_wstring(255, 16), L"ff");
    EXPECT_EQ(wxl::core::to_u16(-0.2 - 1e-16, std::chars_format::general, 12), u"-0.2");
    EXPECT_EQ(wxl::core::to_u16(1.0 / 3.0, std::chars_format::general, 12), u"0.333333333333");
}

TEST(numbers, printing_round_trips) {
    for (const double value : {0.1, 1.0 / 3.0, 1e300, -2.5e-17}) {
        double back = 0;
        EXPECT_TRUE(wxl::core::try_parse(wxl::core::to_string(value), back));
        EXPECT_EQ(back, value);
    }
}

TEST(numbers, printing_with_a_fixed_number_of_digits) {
    std::string out;
    wxl::core::append_fixed(out, 12.3456, 2);
    EXPECT_EQ(out, "12.35");

    out.clear();
    wxl::core::append_fixed(out, 12.0, 0);
    EXPECT_EQ(out, "12");

    std::wstring wide;
    wxl::core::append_fixed(wide, 0.5, 1);
    EXPECT_EQ(wide, L"0.5");
}

TEST(numbers, appending_keeps_what_is_there) {
    std::string out = "page ";
    wxl::core::append_number(out, 7);
    out += " of ";
    wxl::core::append_number(out, 350);

    EXPECT_EQ(out, "page 7 of 350");

    std::wstring wide = L"страница ";
    wxl::core::append_number(wide, 7);
    EXPECT_EQ(wide, L"страница 7");
}

TEST(numbers, printing_in_another_base) {
    std::string out;
    wxl::core::append_number(out, 255, 16);

    EXPECT_EQ(out, "ff");
}

TEST(numbers, scaling_a_byte_count) {
    using wxl::core::byte_unit;

    EXPECT_EQ(wxl::core::scale_bytes(900).unit, byte_unit::bytes);
    EXPECT_EQ(wxl::core::scale_bytes(900).value, 900.0);

    EXPECT_EQ(wxl::core::scale_bytes(2048).unit, byte_unit::kilobytes);
    EXPECT_EQ(wxl::core::scale_bytes(2048).value, 2.0);

    EXPECT_EQ(wxl::core::scale_bytes(3ull * 1024 * 1024).unit, byte_unit::megabytes);
    EXPECT_EQ(wxl::core::scale_bytes(1024ull * 1024 * 1024).unit, byte_unit::gigabytes);

    // Whole numbers stay whole: 1023 bytes is not "1 kilobyte".
    EXPECT_EQ(wxl::core::scale_bytes(1023).unit, byte_unit::bytes);
    EXPECT_EQ(wxl::core::scale_bytes(0).unit, byte_unit::bytes);
}

}  // namespace
