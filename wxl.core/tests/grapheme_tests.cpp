#include <gtest/gtest.h>

#include <cstddef>
#include <format>
#include <fstream>
#include <sstream>
#include <string>
#include <string_view>
#include <vector>

import wxl.core;

namespace {

/// One line of GraphemeBreakTest.txt: the text in UTF-16, and at which unit
/// offsets a letter begins.
struct conformance_case {
    std::u16string text;
    std::vector<char32_t> code_points;
    std::vector<bool> breaks_before;       ///< per code point
    std::vector<std::size_t> boundaries;   ///< unit offsets, the start and the end included
    std::string line;
};

void append_utf16(std::u16string& out, const char32_t code_point) {
    if (code_point < 0x10000) {
        out.push_back(static_cast<char16_t>(code_point));
    } else {
        const char32_t rest = code_point - 0x10000;
        out.push_back(static_cast<char16_t>(0xD800 + (rest >> 10)));
        out.push_back(static_cast<char16_t>(0xDC00 + (rest & 0x3FF)));
    }
}

std::vector<conformance_case> read_conformance() {
    std::ifstream file(WXL_CORE_TESTDATA_DIR "/GraphemeBreakTest.txt", std::ios::binary);
    std::vector<conformance_case> cases;
    std::string line;

    while (std::getline(file, line)) {
        const std::string data = line.substr(0, line.find('#'));
        if (data.find_first_not_of(" \t\r") == std::string::npos) continue;

        conformance_case entry;
        entry.line = line;
        entry.boundaries.push_back(0);

        std::istringstream tokens(data);
        std::string token;
        bool pending_break = false;

        // ÷ and × are three UTF-8 bytes each and stand between hex code points.
        while (tokens >> token) {
            if (token == "\xC3\xB7") {
                pending_break = true;
            } else if (token == "\xC3\x97") {
                pending_break = false;
            } else {
                const auto code_point = static_cast<char32_t>(std::stoul(token, nullptr, 16));
                entry.code_points.push_back(code_point);
                entry.breaks_before.push_back(pending_break);
                if (pending_break && !entry.text.empty()) entry.boundaries.push_back(entry.text.size());
                append_utf16(entry.text, code_point);
            }
        }

        entry.boundaries.push_back(entry.text.size());
        cases.push_back(std::move(entry));
    }

    return cases;
}

const std::vector<conformance_case>& conformance() {
    static const std::vector<conformance_case> cases = read_conformance();
    return cases;
}

TEST(grapheme, the_conformance_test_is_there) {
    // Every data line of the Unicode 18.0.0 file; a short count means the
    // file went missing or the reader stopped understanding it.
    EXPECT_EQ(conformance().size(), 853u);
    EXPECT_EQ(wxl::core::unicode::grapheme_unicode_version(), "18.0.0");
}

TEST(grapheme, the_breaker_agrees_with_every_conformance_case) {
    for (const conformance_case& entry : conformance()) {
        wxl::core::unicode::grapheme_breaker breaker;

        for (std::size_t index = 0; index < entry.code_points.size(); ++index) {
            EXPECT_EQ(breaker.breaks_before(entry.code_points[index]), entry.breaks_before[index])
                << "code point " << index << " of: " << entry.line;
        }
    }
}

TEST(grapheme, stepping_over_utf16_finds_every_boundary) {
    for (const conformance_case& entry : conformance()) {
        std::vector<std::size_t> found{0};

        for (std::size_t at = 0; at < entry.text.size();)
            found.push_back(at = wxl::core::unicode::next_grapheme_boundary(entry.text, at));

        EXPECT_EQ(found, entry.boundaries) << entry.line;
    }
}

TEST(grapheme, the_floor_is_the_last_boundary_not_after_the_offset) {
    for (const conformance_case& entry : conformance()) {
        std::size_t expected = 0;

        for (std::size_t at = 0; at < entry.text.size(); ++at) {
            for (const std::size_t boundary : entry.boundaries)
                if (boundary <= at) expected = boundary;

            EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(entry.text, at), expected)
                << "offset " << at << " of: " << entry.line;
        }

        EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(entry.text, entry.text.size() + 5),
                  entry.text.size());
    }
}

TEST(grapheme, the_letters_a_book_is_made_of) {
    // и with a separate breve, a flag, a family joined by ZWJ, and a
    // Devanagari conjunct: each one letter, and the next letter after it.
    const std::wstring_view short_i = L"\x0438\x0306" L"x";
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(short_i, 0), 2u);
    EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(short_i, 1), 0u);

    const std::wstring_view flags = L"\xD83C\xDDEF\xD83C\xDDF5\xD83C\xDDEB\xD83C\xDDF7";  // JP FR
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(flags, 0), 4u);
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(flags, 4), 8u);
    EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(flags, 6), 4u);

    // MAN ZWJ WOMAN ZWJ GIRL
    const std::wstring_view family = L"\xD83D\xDC68\x200D\xD83D\xDC69\x200D\xD83D\xDC67" L"a";
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(family, 0), 8u);

    // KA VIRAMA SSA
    const std::wstring_view kssa = L"\x0915\x094D\x0937" L"a";
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(kssa, 0), 3u);
}

TEST(grapheme, a_lone_surrogate_is_a_letter_of_its_own) {
    const std::wstring_view broken = L"a\xDC00" L"b";
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(broken, 0), 1u);
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(broken, 1), 2u);
}

/// Where letters begin by the rules alone: one breaker fed the whole text, no
/// restarts, so whatever next and floor skip has to agree with it.
std::vector<std::size_t> boundaries_by_the_rules(const std::u16string_view text) {
    wxl::core::unicode::grapheme_breaker breaker;
    std::vector<std::size_t> boundaries;

    for (std::size_t at = 0; at < text.size();) {
        const std::size_t start = at;
        char32_t code_point = text[at++];

        if (code_point >= 0xD800 && code_point < 0xDC00 && at < text.size() && text[at] >= 0xDC00 &&
            text[at] < 0xE000)
            code_point = 0x10000 + ((code_point - 0xD800) << 10) + (text[at++] - 0xDC00);

        if (breaker.breaks_before(code_point)) boundaries.push_back(start);
    }

    boundaries.push_back(text.size());
    return boundaries;
}

/// The units in hex, for a failure message; gtest builds the message only on
/// failure.
std::string units_of(const std::u16string_view text) {
    std::string units;
    for (const char16_t unit : text) units += std::format(" {:04X}", static_cast<unsigned>(unit));
    return units;
}

/// The same text asked through both UTF-16 views: every step of next from 0,
/// and floor at every offset.
void expect_the_rules(const std::u16string_view text) {
    const std::vector<std::size_t> expected = boundaries_by_the_rules(text);
    const std::wstring wide(text.begin(), text.end());

    std::vector<std::size_t> found{0}, found_wide{0};
    for (std::size_t at = 0; at < text.size();)
        found.push_back(at = wxl::core::unicode::next_grapheme_boundary(text, at));
    for (std::size_t at = 0; at < wide.size();)
        found_wide.push_back(at = wxl::core::unicode::next_grapheme_boundary(std::wstring_view(wide), at));

    ASSERT_EQ(found, expected) << "next over" << units_of(text);
    ASSERT_EQ(found_wide, expected) << "next over wchar_t" << units_of(text);

    std::size_t floor = 0;
    for (std::size_t at = 0; at < text.size(); ++at) {
        for (const std::size_t boundary : expected)
            if (boundary <= at) floor = boundary;

        ASSERT_EQ(wxl::core::unicode::floor_grapheme_boundary(text, at), floor)
            << "floor at " << at << " over" << units_of(text);
        ASSERT_EQ(wxl::core::unicode::floor_grapheme_boundary(std::wstring_view(wide), at), floor)
            << "floor at " << at << " over wchar_t" << units_of(text);
    }
}

TEST(grapheme, plain_neighbours_break_as_the_rules_say) {
    // Every pair up to U+0600 -- past both ranges that need no table, with the
    // marks U+0300..U+036F and U+0483..U+0489 between them and the Hebrew
    // marks after -- so a range that claims too much, or a table that changes
    // under it, shows up as a pair. One view is enough: both overloads are the
    // same template, and the next test asks through wchar_t as well.
    for (char16_t first = 0; first < 0x600; ++first) {
        for (char16_t second = 0; second < 0x600; ++second) {
            wxl::core::unicode::grapheme_breaker breaker;
            breaker.breaks_before(first);
            const bool breaks = breaker.breaks_before(second);

            const char16_t pair[] = {first, second};
            const std::u16string_view text(pair, 2);

            ASSERT_EQ(wxl::core::unicode::next_grapheme_boundary(text, 0), breaks ? 1u : 2u)
                << std::format("{:04X} {:04X}", +first, +second);
            ASSERT_EQ(wxl::core::unicode::floor_grapheme_boundary(text, 1), breaks ? 1u : 0u)
                << std::format("{:04X} {:04X}", +first, +second);
        }
    }
}

TEST(grapheme, beside_marks_joiners_and_jamo_the_rules_decide) {
    // Each code point up to U+0600 before, after and between the classes that
    // join: a mark, ZWJ, a spacing mark, a prepended sign, the five Hangul
    // classes, a regional indicator and a pictograph (both as surrogate
    // pairs), a virama and a consonant, CR and LF.
    const std::u16string_view joining[] = {
        u"\u0301", u"\u200D", u"\u0903", u"\u0600", u"\u1100", u"\u1161", u"\u11A8",
        u"\uAC00", u"\uAC01", u"\U0001F1E6", u"\U0001F600", u"\u094D", u"\u0915", u"\r", u"\n",
    };

    for (char16_t plain = 0; plain < 0x600; ++plain) {
        const std::u16string one(1, plain);

        for (const std::u16string_view other : joining) {
            const std::u16string around(other);

            ASSERT_NO_FATAL_FAILURE(expect_the_rules(around + one));
            ASSERT_NO_FATAL_FAILURE(expect_the_rules(one + around));
            ASSERT_NO_FATAL_FAILURE(expect_the_rules(one + around + one));
            ASSERT_NO_FATAL_FAILURE(expect_the_rules(around + one + one + around));
        }

        // A conjunct carries "after the virama" across InCB=Extend: two plain
        // code points in the middle must not carry it.
        ASSERT_NO_FATAL_FAILURE(expect_the_rules(u"\u0915\u094D" + one + one + u"\u0915"));
    }
}

TEST(grapheme, checked_text_is_asked_as_it_is) {
    constexpr wxl::core::u16_view short_i = u"\x0438\x0306" u"x";
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(short_i, 0), 2u);
    EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(short_i, 1), 0u);

    const wxl::core::u16_text owned{short_i};
    EXPECT_EQ(wxl::core::unicode::next_grapheme_boundary(owned, 2), 3u);
    EXPECT_EQ(wxl::core::unicode::floor_grapheme_boundary(owned, 3), 3u);
}

}  // namespace
