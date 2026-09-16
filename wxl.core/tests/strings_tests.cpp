#include <gtest/gtest.h>

#include <iterator>
#include <string>
#include <string_view>
#include <vector>

import wxl.core;

using namespace std::string_view_literals;

namespace {

std::vector<std::string_view> pieces_of(std::string_view text, std::string_view separator) {
    std::vector<std::string_view> pieces;

    for (const std::string_view piece : wxl::core::split(text, separator))
        pieces.push_back(piece);

    return pieces;
}

TEST(strings, trimming_answers_with_a_view_into_the_text) {
    const std::string text = "  padded  ";
    const std::string_view trimmed = wxl::core::trim(std::string_view(text));

    EXPECT_EQ(trimmed, "padded");
    EXPECT_EQ(trimmed.data(), text.data() + 2);  // a view, not a copy
}

TEST(strings, trimming_the_ends_separately) {
    EXPECT_EQ(wxl::core::trim_front("\t\n x "sv), "x ");
    EXPECT_EQ(wxl::core::trim_back(" x \r\n"sv), " x");
    EXPECT_EQ(wxl::core::trim(" \t\r\n\v\f "sv), ""sv);
    EXPECT_EQ(wxl::core::trim(""sv), ""sv);
    EXPECT_EQ(wxl::core::trim("x"sv), "x");
}

TEST(strings, trimming_a_named_set) {
    EXPECT_EQ(wxl::core::trim("[[value]]"sv, "[]"sv), "value");
    EXPECT_EQ(wxl::core::trim("000"sv, "0"sv), ""sv);
    EXPECT_EQ(wxl::core::trim_front("00123"sv, "0"sv), "123");
}

TEST(strings, trimming_wide_text_too) {
    EXPECT_EQ(wxl::core::trim(L"  1* "sv), L"1*");
}

TEST(strings, case_insensitive_comparison_is_ascii) {
    EXPECT_TRUE(wxl::core::equal_ignore_ascii_case("Microsoft.UI"sv, "microsoft.ui"sv));
    EXPECT_FALSE(wxl::core::equal_ignore_ascii_case("abc"sv, "abcd"sv));

    // Cyrillic is left alone, which is the promise: these are for protocol
    // text, and folding it would need to know the language.
    EXPECT_FALSE(wxl::core::equal_ignore_ascii_case("ПРИВЕТ"sv, "привет"sv));

    EXPECT_TRUE(wxl::core::starts_with_ignore_ascii_case("Content-Type: x"sv, "content-type"sv));
    EXPECT_TRUE(wxl::core::ends_with_ignore_ascii_case("book.FB3"sv, ".fb3"sv));
    EXPECT_FALSE(wxl::core::ends_with_ignore_ascii_case("fb3"sv, ".fb3"sv));
}

TEST(strings, case_conversion) {
    EXPECT_EQ(wxl::core::ascii_lower("Microsoft.WindowsAppSDK"sv), "microsoft.windowsappsdk");
    EXPECT_EQ(wxl::core::ascii_upper("fb3"sv), "FB3");
    EXPECT_EQ(wxl::core::ascii_lower(L"Grid"sv), L"grid");

    // Bytes above ASCII are not letters as far as this is concerned, so UTF-8
    // text survives unchanged rather than being mangled byte by byte.
    EXPECT_EQ(wxl::core::ascii_lower("Дом"sv), "Дом");

    std::string text = "Mixed Case";
    wxl::core::make_ascii_lower(text);
    EXPECT_EQ(text, "mixed case");
    wxl::core::make_ascii_upper(text);
    EXPECT_EQ(text, "MIXED CASE");
}

TEST(strings, splitting_keeps_every_piece) {
    EXPECT_EQ(pieces_of("a,b,c"sv, ","sv), (std::vector<std::string_view>{"a", "b", "c"}));
    EXPECT_EQ(pieces_of("a,,b"sv, ","sv), (std::vector<std::string_view>{"a", "", "b"}));
    EXPECT_EQ(pieces_of(",a,"sv, ","sv), (std::vector<std::string_view>{"", "a", ""}));
    EXPECT_EQ(pieces_of(""sv, ","sv), (std::vector<std::string_view>{""}));
    EXPECT_EQ(pieces_of("nothing to cut"sv, ","sv),
              (std::vector<std::string_view>{"nothing to cut"}));
}

TEST(strings, splitting_on_a_word) {
    EXPECT_EQ(pieces_of("one -- two -- three"sv, " -- "sv),
              (std::vector<std::string_view>{"one", "two", "three"}));

    // The separator is matched whole, so a piece may hold part of it.
    EXPECT_EQ(pieces_of("a--b"sv, " -- "sv), (std::vector<std::string_view>{"a--b"}));
}

TEST(strings, splitting_on_one_character) {
    std::vector<std::string_view> pieces;

    for (const std::string_view piece : wxl::core::split("1*,auto,20"sv, ','))
        pieces.push_back(piece);

    EXPECT_EQ(pieces, (std::vector<std::string_view>{"1*", "auto", "20"}));
}

TEST(strings, a_single_character_separator_is_kept_by_value) {
    // The character arrives by value, so anything holding a view into it would
    // be looking at a dead parameter the moment split() returned -- which a
    // release build notices and a debug build does not.
    const auto pieces = wxl::core::split("a,b,c"sv, ',');

    auto it = pieces.begin();
    ++it;

    EXPECT_EQ(*it, "b");
    EXPECT_EQ(it.rest(), "c");
}

TEST(strings, splitting_hands_out_the_rest_as_well) {
    auto it = wxl::core::split("key=value=with=signs"sv, "="sv).begin();

    EXPECT_EQ(*it, "key");
    EXPECT_EQ(it.rest(), "value=with=signs");
}

TEST(strings, splitting_wide_text) {
    std::vector<std::wstring_view> pieces;

    for (const std::wstring_view piece : wxl::core::split(L"auto,1*,2*"sv, L','))
        pieces.push_back(piece);

    EXPECT_EQ(pieces, (std::vector<std::wstring_view>{L"auto", L"1*", L"2*"}));
}

TEST(strings, joining) {
    const std::vector<std::string_view> names = {"one", "two", "three"};

    EXPECT_EQ(wxl::core::join(names, ", "sv), "one, two, three");
    EXPECT_EQ(wxl::core::join(std::vector<std::string_view>{}, ", "sv), "");
    EXPECT_EQ(wxl::core::join(std::vector<std::string_view>{"alone"}, ", "sv), "alone");

    const std::vector<std::string> owned = {"a", "b"};
    EXPECT_EQ(wxl::core::join(owned, "/"sv), "a/b");

    const std::vector<std::wstring_view> wide = {L"a", L"b"};
    EXPECT_EQ(wxl::core::join(wide, L" - "sv), L"a - b");
}

TEST(strings, joining_appends_to_what_is_there) {
    std::string out = "names: ";
    wxl::core::append_joined(out, std::vector<std::string_view>{"x", "y"}, ", "sv);

    EXPECT_EQ(out, "names: x, y");
}

TEST(strings, escaping_only_what_cannot_stand) {
    EXPECT_EQ(wxl::core::xml_escaped("plain text"sv), "plain text");
    EXPECT_EQ(wxl::core::xml_escaped("a & b"sv), "a &amp; b");
    EXPECT_EQ(wxl::core::xml_escaped("<tag attr=\"x\">"sv), "&lt;tag attr=&quot;x&quot;&gt;");
    EXPECT_EQ(wxl::core::xml_escaped("&&"sv), "&amp;&amp;");
    EXPECT_EQ(wxl::core::xml_escaped(""sv), "");

    // UTF-8 goes through untouched: XML carries it as itself.
    EXPECT_EQ(wxl::core::xml_escaped("Пушкин & Гоголь"sv), "Пушкин &amp; Гоголь");

    std::string out = "<v>";
    wxl::core::append_xml_escaped(out, "1 < 2"sv);
    out += "</v>";
    EXPECT_EQ(out, "<v>1 &lt; 2</v>");
}

TEST(strings, ascii_check) {
    EXPECT_TRUE(wxl::core::is_ascii(""sv));
    EXPECT_TRUE(wxl::core::is_ascii("the quick brown fox jumps over it"sv));
    EXPECT_FALSE(wxl::core::is_ascii("дом"sv));

    // The check walks whole words first, so the interesting case is a byte
    // past the last whole one.
    EXPECT_FALSE(wxl::core::is_ascii("aaaaaaaaaaaaaaaaд"sv));
}

TEST(strings, character_classes) {
    EXPECT_TRUE(wxl::core::is_ascii_space(' '));
    EXPECT_TRUE(wxl::core::is_ascii_space(L'\n'));
    EXPECT_FALSE(wxl::core::is_ascii_space('x'));
    EXPECT_TRUE(wxl::core::is_ascii_digit('7'));
    EXPECT_FALSE(wxl::core::is_ascii_digit('a'));
    EXPECT_EQ(wxl::core::to_ascii_lower('A'), 'a');
    EXPECT_EQ(wxl::core::to_ascii_upper(L'z'), L'Z');
}

}  // namespace
