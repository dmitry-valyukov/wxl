#include <gtest/gtest.h>

#include <filesystem>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

import wxl.core;

using namespace std::string_view_literals;

namespace {

// The transcoders take text somebody has vouched for. Here that somebody is
// the compiler: every literal below is written into the source, so it is
// UTF-8 -- or UTF-16 -- by construction, and assume_valid() is where that
// argument is recorded rather than assumed.
using wxl::core::assume_valid;

// The transcoders walk eight bytes at a time, so the interesting lengths are
// the ones around that step: a text shorter than a word, one exactly a word
// long, and one whose tail falls after the last whole word.
constexpr std::string_view ascii = "The quick brown fox jumps over the lazy dog"sv;
constexpr std::string_view russian = "Съешь ещё этих мягких французских булок"sv;
constexpr std::string_view mixed = "a б в 字 😀 z"sv;

TEST(unicode, ascii_round_trip) {
    const wxl::core::u16_text wide = assume_valid(ascii).to_utf16();

    EXPECT_EQ(wide.size(), ascii.size());
    EXPECT_EQ(wide.to_utf8(), ascii);
}

TEST(unicode, russian_round_trip) {
    const wxl::core::u16_text wide = assume_valid(russian).to_utf16();

    EXPECT_EQ(wide.to_utf8(), russian);
    EXPECT_EQ(wide.size(), wxl::core::code_point_count(assume_valid(russian)));
}

TEST(unicode, surrogate_pair_round_trip) {
    const std::string smile = "\xF0\x9F\x98\x80";  // U+1F600

    EXPECT_EQ(wxl::core::code_point_count(assume_valid(smile)), 1u);
    EXPECT_EQ(wxl::core::utf16_size(assume_valid(smile)), 2u);

    const wxl::core::u16_text wide = assume_valid(smile).to_utf16();

    ASSERT_EQ(wide.size(), 2u);
    EXPECT_TRUE(wxl::core::is_high_surrogate(wide.wchars()[0]));
    EXPECT_TRUE(wxl::core::is_low_surrogate(wide.wchars()[1]));
    EXPECT_EQ(wide.to_utf8(), smile);
}

TEST(unicode, round_trip_at_every_length) {
    // Growing prefixes cross the eight-byte step at every offset, and a prefix
    // is cut on a code point boundary rather than mid-sequence.
    std::string text;

    for (const char32_t code_point : {U'a', U'б', U'字', U'\U0001F600'})
        for (int repeat = 0; repeat != 4; ++repeat) wxl::core::append_utf8(text, code_point);

    for (std::size_t size = 0; size != text.size(); ++size) {
        if (size != text.size() && wxl::core::is_continuation(text[size])) continue;

        const std::string_view prefix = std::string_view(text).substr(0, size);

        EXPECT_TRUE(wxl::core::is_valid_utf8(prefix)) << "at " << size;
        EXPECT_EQ(assume_valid(prefix).to_utf16().to_utf8(), prefix) << "at " << size;
    }
}

TEST(unicode, sizes_are_what_the_conversion_writes) {
    for (const std::string_view text : {ascii, russian, mixed}) {
        const wxl::core::u16_text wide = assume_valid(text).to_utf16();

        EXPECT_EQ(wide.size(), wxl::core::utf16_size(assume_valid(text)));
        EXPECT_EQ(text.size(), wxl::core::utf8_size(wide));
    }
}

TEST(unicode, code_point_sizes) {
    EXPECT_EQ(wxl::core::utf8_size(U'a'), 1);
    EXPECT_EQ(wxl::core::utf8_size(U'б'), 2);
    EXPECT_EQ(wxl::core::utf8_size(U'字'), 3);
    EXPECT_EQ(wxl::core::utf8_size(U'\U0001F600'), 4);

    EXPECT_EQ(wxl::core::utf16_size(U'字'), 1);
    EXPECT_EQ(wxl::core::utf16_size(U'\U0001F600'), 2);
}

TEST(unicode, counting_characters_not_bytes) {
    EXPECT_EQ(wxl::core::code_point_count(assume_valid(""sv)), 0u);
    EXPECT_EQ(wxl::core::code_point_count(assume_valid(ascii)), ascii.size());
    EXPECT_EQ(wxl::core::code_point_count(assume_valid("привет"sv)), 6u);
    EXPECT_EQ(wxl::core::code_point_count(assume_valid("a😀b"sv)), 3u);
}

TEST(unicode, valid_utf8_is_accepted) {
    for (const std::string_view text : {""sv, ascii, russian, mixed})
        EXPECT_FALSE(wxl::core::find_invalid_utf8(text).has_value());
}

TEST(unicode, broken_utf8_is_named_by_its_offset) {
    struct {
        std::string_view text;
        std::size_t offset;
    } const cases[] = {
        {"ab\x80z"sv, 2},                  // a continuation byte with nothing to continue
        {"ab\xC0\xAFz"sv, 2},              // over-long "/"
        {"ab\xE0\x80\xAFz"sv, 2},          // over-long again, three bytes this time
        {"ab\xED\xA0\x80z"sv, 2},          // U+D800, a surrogate half
        {"ab\xF5\x80\x80\x80z"sv, 2},      // above U+10FFFF
        {"ab\xD0"sv, 2},                   // cut off before its continuation
        {"ab\xD0z"sv, 2},                  // continuation that is not one
        {"ab\xFE\xFFz"sv, 2},              // bytes UTF-8 never uses at all
    };

    for (const auto& one : cases) {
        const auto found = wxl::core::find_invalid_utf8(one.text);

        ASSERT_TRUE(found.has_value()) << one.text;
        EXPECT_EQ(*found, one.offset) << one.text;
    }
}

TEST(unicode, the_smallest_and_the_largest) {
    std::string text;
    wxl::core::append_utf8(text, U'\0');
    wxl::core::append_utf8(text, static_cast<char32_t>(wxl::core::max_code_point));

    EXPECT_TRUE(wxl::core::is_valid_utf8(text));
    EXPECT_EQ(wxl::core::code_point_count(assume_valid(text)), 2u);
    EXPECT_EQ(assume_valid(text).to_utf16().to_utf8(), text);
}

TEST(unicode, broken_utf16_is_named_by_its_offset) {
    const std::wstring lone_high = L"ab\xD800";
    const std::wstring lone_low = L"ab\xDC00z";
    const std::wstring high_then_letter = L"ab\xD800z";

    EXPECT_EQ(wxl::core::find_invalid_utf16(lone_high), wxl::core::nullable<std::size_t>(2));
    EXPECT_EQ(wxl::core::find_invalid_utf16(lone_low), wxl::core::nullable<std::size_t>(2));
    EXPECT_EQ(wxl::core::find_invalid_utf16(high_then_letter), wxl::core::nullable<std::size_t>(2));

    EXPECT_TRUE(wxl::core::is_valid_utf16(L"ab\xD83D\xDE00z"));
}

TEST(unicode, walking_hands_out_position_and_bytes) {
    const std::string text = "a😀б";

    std::vector<char32_t> seen;
    std::vector<std::size_t> positions;
    std::vector<std::string_view> sequences;

    const wxl::core::code_points walk(assume_valid(text));

    for (auto it = walk.begin(); it != walk.end(); ++it) {
        seen.push_back(*it);
        positions.push_back(it.position());
        sequences.push_back(it.sequence().chars());
    }

    EXPECT_EQ(seen, (std::vector<char32_t>{U'a', U'\U0001F600', U'б'}));
    EXPECT_EQ(positions, (std::vector<std::size_t>{0, 1, 5}));
    EXPECT_EQ(sequences[1].size(), 4u);
    EXPECT_EQ(sequences[2].size(), 2u);
}

TEST(unicode, walking_works_in_a_range_for) {
    std::size_t count = 0;

    for (const char32_t code_point : wxl::core::code_points(assume_valid(russian))) {
        EXPECT_TRUE(wxl::core::is_scalar_value(code_point));
        ++count;
    }

    EXPECT_EQ(count, wxl::core::code_point_count(assume_valid(russian)));
}

TEST(unicode, appending_grows_what_is_already_there) {
    std::wstring wide = L"<<";
    wxl::core::append_utf16(wide, assume_valid(russian));
    wxl::core::append_utf16(wide, U'>');

    const wxl::core::u16_text wide_russian = assume_valid(russian).to_utf16();

    EXPECT_EQ(wide, L"<<" + std::wstring(wide_russian.wchars()) + L">");

    std::string narrow = "<<";
    wxl::core::append_utf8(narrow, wide_russian);

    EXPECT_EQ(narrow, "<<" + std::string(russian));
}

}  // namespace

// The platform this tree is written for, asserted once here rather than at
// every reinterpret_cast that relies on it. UTF-16 text is handed to Windows
// as wchar_t -- wchars(), c_str() and assume_valid(std::wstring_view) all read
// the same units under another name, and that reading is exactly this
// equality. A tree compiled where wchar_t is four bytes would need real
// conversions there instead, and this is where it would say so.
static_assert(sizeof(wchar_t) == sizeof(char16_t),
              "UTF-16 text is handed to Windows as wchar_t; this tree is Windows only");
static_assert(alignof(wchar_t) == alignof(char16_t));
