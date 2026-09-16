#include <gtest/gtest.h>

import wxl.xml;

using namespace wxl::xml;

TEST(xml_encoding, well_formed_text_is_accepted) {
    EXPECT_NO_THROW(validate_utf8(""));
    EXPECT_NO_THROW(validate_utf8("ascii, кириллица, ①, \U0001F600"));

    // Long enough to run past the eight-bytes-at-a-time path several times,
    // with the non-ASCII sequence at a different offset within a word each
    // time round.
    for (std::size_t offset = 0; offset < 16; ++offset) {
        const sta_string text = sta_string(offset, 'a') + "\U0001F600" + sta_string(32, 'b');

        EXPECT_NO_THROW(validate_utf8(text)) << "at offset " << offset;
    }
}

TEST(xml_encoding, malformed_input_is_refused) {
    EXPECT_THROW(validate_utf8("\xC3"), exception);              // truncated
    EXPECT_THROW(validate_utf8("\x80"), exception);              // a continuation on its own
    EXPECT_THROW(validate_utf8("\xC0\xAF"), exception);          // over-long '/'
    EXPECT_THROW(validate_utf8("\xED\xA0\x80"), exception);      // a surrogate
    EXPECT_THROW(validate_utf8("\xF5\x80\x80\x80"), exception);  // beyond U+10FFFF

    // Past the ASCII fast path, so the broken sequence is found by the slow
    // one rather than by the very first word test.
    EXPECT_THROW(validate_utf8(std::string(64, 'a') + "\x80"), exception);
}

// Writing a code point, turning a path into UTF-8 and the rest of the encoding
// belong to wxl::core now, and are tested there. What is left here is the one
// thing this module adds: the complaint.
