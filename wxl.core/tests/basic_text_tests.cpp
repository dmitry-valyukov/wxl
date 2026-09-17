#include <gtest/gtest.h>

#include <crtdbg.h>
#include <stdlib.h>

#include <string>
#include <string_view>
#include <type_traits>

import wxl.core;

using namespace std::string_view_literals;

using wxl::core::unicode::assume_valid;
using wxl::core::unicode::checked;
using wxl::core::u16_text;
using wxl::core::u16_view;
using wxl::core::u8_text;
using wxl::core::u8_view;

namespace {

// What the type is for: text cannot be called checked by accident. The way out
// costs nothing, since dropping a guarantee breaks nothing: checked text is
// plain char or wchar_t text whenever that is what is asked for.
static_assert(!std::is_constructible_v<u8_view, std::u8string_view>);
static_assert(!std::is_constructible_v<u8_view, std::string_view>);
static_assert(!std::is_convertible_v<u8_view, std::u8string_view>);
static_assert(!std::is_convertible_v<std::u8string_view, u8_view>);

static_assert(std::is_convertible_v<u8_view, std::string_view>);
static_assert(std::is_convertible_v<u8_view, std::string>);
static_assert(std::is_convertible_v<u8_text, std::string_view>);
static_assert(std::is_convertible_v<u8_text, std::string>);
static_assert(std::is_convertible_v<u16_view, std::wstring_view>);
static_assert(std::is_convertible_v<u16_view, std::wstring>);
static_assert(std::is_convertible_v<u16_text, std::wstring_view>);
static_assert(std::is_convertible_v<u16_text, std::wstring>);

// A string that owns its text answers as a view of the same text, and the view
// is safe as well -- which is what lets checked text be passed on.
static_assert(std::is_convertible_v<u8_text, u8_view>);
static_assert(std::is_convertible_v<u16_text, u16_view>);

// A string grows by code points and never by units: half a pair or a stray
// continuation byte is what the type rules out, so a unit does not compile.
template <typename Text, typename Unit>
constexpr bool grows_by = requires(Text text, Unit unit) { text.push_back(unit); };

static_assert(grows_by<u8_text, char32_t>);
static_assert(grows_by<u16_text, char32_t>);
static_assert(!grows_by<u8_text, char>);
static_assert(!grows_by<u8_text, char8_t>);
static_assert(!grows_by<u16_text, char16_t>);
static_assert(!grows_by<u16_text, wchar_t>);

// A literal is checked where it is written, so it needs no call at all.
constexpr u8_view greeting = u8"Здравствуйте, 😀";
constexpr u16_view wide_greeting = u"Здравствуйте, 😀";

// The broken UTF-16 below is built a unit at a time rather than written as an
// escape: a lone surrogate in a wide literal is exactly the thing a compiler
// feels entitled to reinterpret, and a test of broken text has to be sure it
// is testing the text it meant.
std::wstring with_unit(std::wstring_view before, unsigned code_unit, std::wstring_view after) {
    std::wstring text(before);
    text.push_back(static_cast<wchar_t>(code_unit));
    text.append(after);
    return text;
}

/// The same text with U+FFFD where the broken unit was.
std::string replaced(std::string_view before, std::string_view after) {
    std::string text(before);
    wxl::core::unicode::append_utf8(text, wxl::core::unicode::replacement_character);
    text.append(after);
    return text;
}

TEST(basic_text, an_empty_text_is_valid) {
    constexpr u8_view nothing;

    EXPECT_TRUE(nothing.empty());
    EXPECT_EQ(nothing.size(), 0u);
    EXPECT_EQ(nothing, u8_view{});
}

TEST(basic_text, a_literal_is_checked_where_it_is_written) {
    // No call, no optional, nothing to forget: the conversion happened in the
    // compiler, and a mis-encoded literal would not have got this far.
    EXPECT_EQ(greeting.chars(), "Здравствуйте, 😀"sv);
    EXPECT_EQ(wxl::core::unicode::code_point_count(greeting), 15u);

    EXPECT_EQ(wide_greeting.wchars(), L"Здравствуйте, 😀"sv);

    // And the check really is a check -- the same walk the run-time door does.
    static_assert(!wxl::core::unicode::find_invalid_utf8(u8"Съешь ещё"sv).has_value());
    static_assert(wxl::core::unicode::find_invalid_utf8(std::string_view("ab\x80z")).has_value());
    static_assert(wxl::core::unicode::checked(u8"привет"sv).has_value());
}

TEST(basic_text, checked_accepts_utf8_and_hands_the_text_back) {
    const auto text = checked("Съешь ещё этих мягких булок"sv);

    ASSERT_TRUE(text.has_value());
    EXPECT_EQ(*text, "Съешь ещё этих мягких булок"sv);
    EXPECT_EQ(wxl::core::unicode::code_point_count(*text), 27u);
}

TEST(basic_text, checked_refuses_broken_utf8) {
    for (const std::string_view broken : {
             "ab\x80z"sv,              // a continuation byte with nothing to continue
             "ab\xC0\xAFz"sv,          // over-long "/"
             "ab\xED\xA0\x80z"sv,      // U+D800, a surrogate half
             "ab\xF5\x80\x80\x80z"sv,  // above U+10FFFF
             "ab\xD0"sv,               // cut off before its continuation
         })
        EXPECT_FALSE(checked(broken).has_value()) << broken;
}

TEST(basic_text, checked_refuses_a_lone_surrogate) {
    const std::wstring pair = with_unit(with_unit(L"ab", 0xD83D, L""), 0xDE00, L"z");

    EXPECT_TRUE(checked(std::wstring_view(pair)).has_value());
    EXPECT_FALSE(checked(std::wstring_view(with_unit(L"ab", 0xD800, L""))).has_value());
    EXPECT_FALSE(checked(std::wstring_view(with_unit(L"ab", 0xDC00, L"z"))).has_value());
}

TEST(basic_text, checked_reads_both_spellings_of_the_same_bytes) {
    EXPECT_EQ(checked("привет"sv)->chars(), checked(u8"привет"sv)->chars());
    EXPECT_EQ(checked(L"привет"sv)->wchars(), checked(u"привет"sv)->wchars());
}

TEST(basic_text, comparing_needs_no_unwrapping) {
    // What keeps `node.name() == "section"` reading the way it always did.
    EXPECT_TRUE(greeting == "Здравствуйте, 😀"sv);
    EXPECT_FALSE(greeting == "нет"sv);
    EXPECT_TRUE(wide_greeting == L"Здравствуйте, 😀"sv);

    const u8_text owned{greeting};

    EXPECT_TRUE(owned == "Здравствуйте, 😀"sv);
    EXPECT_TRUE(u8_view(owned) == greeting);
    EXPECT_TRUE(owned == greeting);
    EXPECT_TRUE(greeting == owned);

    const u16_text wide_owned{wide_greeting};

    EXPECT_TRUE(wide_owned == wide_greeting);
    EXPECT_TRUE(wide_greeting == wide_owned);
}

TEST(basic_text, checked_text_is_plain_text_where_plain_text_is_asked_for) {
    const auto narrow = [](std::string_view text) { return text.size(); };
    const auto wide = [](std::wstring_view text) { return text.size(); };

    const u8_text owned{greeting};
    const u16_text wide_owned{wide_greeting};

    EXPECT_EQ(narrow(greeting), greeting.size());
    EXPECT_EQ(narrow(owned), owned.size());
    EXPECT_EQ(wide(wide_greeting), wide_greeting.size());
    EXPECT_EQ(wide(wide_owned), wide_owned.size());

    const std::string copied = greeting;
    const std::wstring wide_copied = wide_owned;

    EXPECT_EQ(copied, "Здравствуйте, 😀"sv);
    EXPECT_EQ(wide_copied, L"Здравствуйте, 😀"sv);
}

TEST(basic_text, the_text_is_borrowed_and_not_copied) {
    const std::string text = "a😀b";
    const u8_view view = assume_valid(text);

    // Same bytes, not a copy of them: chars() is a reinterpretation and
    // nothing more, which is the reason char8_t underneath costs nothing.
    EXPECT_EQ(view.chars().data(), text.data());
    EXPECT_EQ(view.size(), text.size());
}

TEST(basic_text, a_part_of_checked_text_is_checked_text) {
    // Two bytes for the 'ё', four for the emoji: the offsets below are the
    // boundaries between code points, and cutting at one leaves both halves as
    // well-formed as the whole was.
    constexpr u8_view text = u8"ёж😀";

    EXPECT_TRUE(text.substr(0, 4) == "ёж"sv);
    EXPECT_TRUE(text.substr(4) == "😀"sv);
    EXPECT_TRUE(text.substr(text.size()).empty());

    // Still the same bytes, not a copy of them.
    EXPECT_EQ(text.substr(2).chars().data(), text.chars().data() + 2);
}

TEST(basic_text, a_part_is_cut_at_compile_time_too) {
    // Nothing here runs: a literal is checked by the compiler and so is the
    // part cut out of it, which is what constexpr on substr() buys.
    constexpr u8_view text = u8"ёж😀";
    static_assert(text.substr(0, 4) == u8"ёж"sv);
    static_assert(text.substr(4).size() == 4);

    constexpr u16_view wide = u"ёж😀";
    static_assert(wide.substr(0, 2) == u"ёж"sv);
    static_assert(wide.substr(2).size() == 2);  // the surrogate pair, both halves
}

TEST(basic_text, a_string_grows_by_whole_code_points) {
    u8_text narrow;
    u16_text wide;

    for (const char32_t code_point : {U'a', U'ё', U'𠮷', U'😀'}) {
        narrow.push_back(code_point);
        wide.push_back(code_point);
    }

    EXPECT_EQ(narrow.chars(), "aё𠮷😀"sv);
    EXPECT_EQ(narrow.size(), 11u);  // 1 + 2 + 4 + 4
    EXPECT_EQ(wide.wchars(), L"aё𠮷😀"sv);
    EXPECT_EQ(wide.size(), 6u);     // 1 + 1 + a pair + a pair
}

TEST(basic_text, a_string_grows_by_checked_text) {
    u16_text title{u"Отцы"};
    title += u" и ";
    title.append(wide_greeting.substr(14));

    EXPECT_EQ(title.wchars(), L"Отцы и 😀"sv);

    u8_text line;
    line.reserve(64);
    line += greeting.substr(0, 24);
    line += u8", ";
    line += u8_text{u8"мир"};

    EXPECT_EQ(line.chars(), "Здравствуйте, мир"sv);
}

TEST(basic_text, a_string_is_cut_between_code_points) {
    u8_text text{u8"ёж😀  "};

    // What trimming looks like: the offset found in the plain units, the cut
    // made on the text.
    text.erase(text.plain().find_last_not_of(u8' ') + 1);
    EXPECT_EQ(text.chars(), "ёж😀"sv);

    text.erase(2, 2);
    EXPECT_EQ(text.chars(), "ё😀"sv);

    u16_text wide{u"ёж😀"};
    wide.erase(2);
    EXPECT_EQ(wide.wchars(), L"ёж"sv);
}

// Cutting a code point in half is the caller's bug -- the offset came from its
// own walk through text it was handed whole -- so it takes the process down in
// every build, and to stderr rather than to a dialog that would hang the run.
void report_failures_to_stderr() {
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
}

void cut_a_character_in_half() {
    report_failures_to_stderr();

    constexpr u8_view text = u8"ёж";

    (void)text.substr(1);
}

void grow_by_half_a_pair() {
    report_failures_to_stderr();

    u16_text text;
    text.push_back(char32_t{0xD83D});
}

void erase_half_a_pair() {
    report_failures_to_stderr();

    u16_text text{u"ёж😀"};
    text.erase(3);
}

void erase_a_middle_byte() {
    report_failures_to_stderr();

    u8_text text{u8"ёж"};
    text.erase(0, 1);
}

TEST(basic_text_death_tests, a_cut_inside_a_code_point_takes_the_process_down) {
    EXPECT_DEATH(cut_a_character_in_half(), "");
}

TEST(basic_text_death_tests, a_string_refuses_what_is_not_a_code_point) {
    EXPECT_DEATH(grow_by_half_a_pair(), "");
}

TEST(basic_text_death_tests, a_string_is_not_erased_through_a_code_point) {
    EXPECT_DEATH(erase_half_a_pair(), "");
    EXPECT_DEATH(erase_a_middle_byte(), "");
}

TEST(basic_text, asking_for_the_encoding_it_already_has_costs_nothing) {
    // The point of the identity pair: a caller that does not know which of the
    // two it holds writes to_utf8() or to_utf16() and is right either way.
    EXPECT_EQ(greeting.to_utf8().data(), greeting.data());

    const u8_text owned{greeting};

    EXPECT_EQ(&owned.to_utf8(), &owned);
}

TEST(basic_text, transcoding_keeps_the_guarantee_and_the_text) {
    const u16_text wide = greeting.to_utf16();

    EXPECT_EQ(wide, wide_greeting.plain());
    EXPECT_EQ(wide.to_utf8(), greeting.plain());

    // Null-terminated for the Windows calls that take a pointer and no length.
    EXPECT_EQ(std::wstring_view(wide.c_str()), L"Здравствуйте, 😀"sv);
    EXPECT_EQ(std::string_view(wide.to_utf8().c_str()), "Здравствуйте, 😀"sv);
}

TEST(basic_text, a_string_owns_its_text_and_still_answers_as_a_view) {
    const u8_text owned{assume_valid("мягких булок"sv)};

    EXPECT_EQ(owned.chars(), "мягких булок"sv);

    // Implicitly, because the guarantee travels with it: this call takes a
    // u8_view and gets one without anybody writing a conversion.
    EXPECT_EQ(wxl::core::unicode::code_point_count(owned), 12u);

    const u8_view view = owned;

    EXPECT_EQ(view.chars(), owned.chars());
}

TEST(basic_text, a_string_gives_up_its_text_when_asked) {
    u8_text owned{assume_valid("мягких"sv)};

    const std::u8string text = std::move(owned).plain();

    // Read back through assume_valid rather than compared as a std::u8string:
    // the gtest in vcpkg is built without char8_t printing, so an EXPECT_EQ on
    // one fails to link rather than to compile.
    EXPECT_EQ(assume_valid(text).chars(), "мягких"sv);
    EXPECT_TRUE(owned.empty());  // still a valid basic_text, just an empty one
}

TEST(basic_text, a_lone_surrogate_becomes_a_replacement_character) {
    // What a broken file name has to turn into on its way into a UTF-8 file.
    // Passing it on unchecked is what this exists to prevent: transcoding the
    // same text writes ED B0 80, which no reader of UTF-8 accepts, and drops
    // the tail as well -- the size pass counts a surrogate half as one unit of
    // a pair that is not there.
    const u8_text bytes = wxl::core::unicode::repaired(with_unit(L"a", 0xDC00, L"z")).to_utf8();

    EXPECT_EQ(bytes, replaced("a", "z"));
    EXPECT_FALSE(wxl::core::unicode::find_invalid_utf8(bytes.chars()).has_value());
}

TEST(basic_text, a_lone_high_surrogate_at_the_end_is_replaced_too) {
    const u8_text bytes = wxl::core::unicode::repaired(with_unit(L"a", 0xD800, L"")).to_utf8();

    EXPECT_EQ(bytes, replaced("a", ""));
}

TEST(basic_text, repairing_leaves_well_formed_text_alone) {
    const std::wstring pair = with_unit(with_unit(L"a", 0xD83D, L""), 0xDE00, L"z");

    for (const std::wstring_view text : {L"привет"sv, std::wstring_view(pair), L""sv})
        EXPECT_EQ(wxl::core::unicode::repaired(text), assume_valid(text).plain());
}

TEST(basic_text, a_path_is_utf8_whatever_the_code_page) {
    const std::filesystem::path path = L"m:/wxl/тест.xml";

    EXPECT_EQ(wxl::core::unicode::to_utf8(path), "m:/wxl/тест.xml"sv);
}

}  // namespace
