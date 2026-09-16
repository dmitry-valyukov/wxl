#include <gtest/gtest.h>

#include <filesystem>
#include <vector>

#include "config.h"

import wxl.core;
import wxl.xml;

using namespace wxl::xml;

// A diagnostic needs the file and the line: the line comes from the node,
// the file from the document, which is the one place the name is kept.
TEST(xml, file_name) {
    const std::filesystem::path path = WXL_XML_TEST_DATA_DIR "filename.xml";

    document p;
    const node& root = p.load_file(path);

    EXPECT_EQ(p.file_name(), wxl::core::to_utf8(path).chars());

    const node* entries = root.find("Entries");
    ASSERT_NE(entries, nullptr);

    const std::vector<int> expected_lines = {6, 7, 8, 11, 14, 19};
    std::vector<int> lines;

    for (const auto& entry : entries->children_named("Entry"))
        lines.push_back(entry.line());

    EXPECT_EQ(lines, expected_lines);
}

// An node is reported at the '<' it opens with -- where a reader looking
// for it would put the cursor.
TEST(xml, position_of_an_element) {
    document p;
    const node& root = p.load("<a>\n  <b/>\n</a>");

    EXPECT_EQ(root.line(), 1);
    EXPECT_EQ(root.column(), 1);

    const node* b = root("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->line(), 2);
    EXPECT_EQ(b->column(), 3);
}

TEST(xml, a_document_that_was_not_named) {
    document p;
    p.load("<a/>");

    EXPECT_TRUE(p.file_name().empty());
}

// A document that does not parse names itself in the shape a compiler reports
// a position in, so an editor can open it there from the message alone. Beside
// that, and answering a different question, the rule that gave up.
TEST(xml, a_parsing_error_names_the_document_and_the_place) {
    document p;

    try {
        p.load("<a>\n  <b>\n</a>", "broken.xml");
        FAIL() << "a document that does not parse has to throw";
    } catch (const parsing_exception& e) {
        EXPECT_EQ(e.file_name(), "broken.xml");
        EXPECT_EQ(e.line(), 2);
        EXPECT_EQ(e.column(), 3);
        EXPECT_TRUE(std::string_view(e.what()).starts_with("broken.xml(2,3)")) << "the message was: " << e.what();
        EXPECT_TRUE(std::string_view(e.rule().file_name()).ends_with("parser.cpp"))
            << "the rule was in: " << e.rule().file_name();
    }
}

// Without a name there is still a position, and the message says so rather
// than pretending to a file.
TEST(xml, a_parsing_error_in_a_document_that_was_not_named) {
    document p;

    try {
        p.load("<a></b>");
        FAIL() << "a document that does not parse has to throw";
    } catch (const parsing_exception& e) {
        EXPECT_TRUE(e.file_name().empty());
        EXPECT_TRUE(std::string_view(e.what()).starts_with("(1,")) << "the message was: " << e.what();
    }
}
