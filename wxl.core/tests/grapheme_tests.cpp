#include <gtest/gtest.h>

#include <cstddef>
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
    EXPECT_EQ(wxl::core::grapheme_unicode_version(), "18.0.0");
}

TEST(grapheme, the_breaker_agrees_with_every_conformance_case) {
    for (const conformance_case& entry : conformance()) {
        wxl::core::grapheme_breaker breaker;

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
            found.push_back(at = wxl::core::next_grapheme_boundary(entry.text, at));

        EXPECT_EQ(found, entry.boundaries) << entry.line;
    }
}

TEST(grapheme, the_floor_is_the_last_boundary_not_after_the_offset) {
    for (const conformance_case& entry : conformance()) {
        std::size_t expected = 0;

        for (std::size_t at = 0; at < entry.text.size(); ++at) {
            for (const std::size_t boundary : entry.boundaries)
                if (boundary <= at) expected = boundary;

            EXPECT_EQ(wxl::core::floor_grapheme_boundary(entry.text, at), expected)
                << "offset " << at << " of: " << entry.line;
        }

        EXPECT_EQ(wxl::core::floor_grapheme_boundary(entry.text, entry.text.size() + 5),
                  entry.text.size());
    }
}

TEST(grapheme, the_letters_a_book_is_made_of) {
    // и with a separate breve, a flag, a family joined by ZWJ, and a
    // Devanagari conjunct: each one letter, and the next letter after it.
    const std::wstring_view short_i = L"\x0438\x0306" L"x";
    EXPECT_EQ(wxl::core::next_grapheme_boundary(short_i, 0), 2u);
    EXPECT_EQ(wxl::core::floor_grapheme_boundary(short_i, 1), 0u);

    const std::wstring_view flags = L"\xD83C\xDDEF\xD83C\xDDF5\xD83C\xDDEB\xD83C\xDDF7";  // JP FR
    EXPECT_EQ(wxl::core::next_grapheme_boundary(flags, 0), 4u);
    EXPECT_EQ(wxl::core::next_grapheme_boundary(flags, 4), 8u);
    EXPECT_EQ(wxl::core::floor_grapheme_boundary(flags, 6), 4u);

    // MAN ZWJ WOMAN ZWJ GIRL
    const std::wstring_view family = L"\xD83D\xDC68\x200D\xD83D\xDC69\x200D\xD83D\xDC67" L"a";
    EXPECT_EQ(wxl::core::next_grapheme_boundary(family, 0), 8u);

    // KA VIRAMA SSA
    const std::wstring_view kssa = L"\x0915\x094D\x0937" L"a";
    EXPECT_EQ(wxl::core::next_grapheme_boundary(kssa, 0), 3u);
}

TEST(grapheme, a_lone_surrogate_is_a_letter_of_its_own) {
    const std::wstring_view broken = L"a\xDC00" L"b";
    EXPECT_EQ(wxl::core::next_grapheme_boundary(broken, 0), 1u);
    EXPECT_EQ(wxl::core::next_grapheme_boundary(broken, 1), 2u);
}

TEST(grapheme, checked_text_is_asked_as_it_is) {
    constexpr wxl::core::u16_view short_i = u"\x0438\x0306" u"x";
    EXPECT_EQ(wxl::core::next_grapheme_boundary(short_i, 0), 2u);
    EXPECT_EQ(wxl::core::floor_grapheme_boundary(short_i, 1), 0u);

    const wxl::core::u16_text owned{short_i};
    EXPECT_EQ(wxl::core::next_grapheme_boundary(owned, 2), 3u);
    EXPECT_EQ(wxl::core::floor_grapheme_boundary(owned, 3), 3u);
}

}  // namespace
