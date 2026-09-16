#include <gtest/gtest.h>

#include <set>
#include <string_view>

#include "config.h"

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

namespace {

std::set<std::string_view> entry_ids(const node& entries) {
    std::set<std::string_view> ids;

    for (const auto& entry : entries.children()) {
        if (entry.name() == "Entry")
            ids.insert(entry.required_attribute("id").chars());
    }

    return ids;
}

}  // namespace

// The document is written with CRLF endings, and nothing may swallow the CR:
// every name and value the tree hands out is a view into those very bytes.
TEST(xml, carriage_return) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "CRLF.xml");

    const node* entries = root.find("Entries");
    ASSERT_NE(entries, nullptr);

    const std::set<std::string_view> ids = entry_ids(*entries);

    EXPECT_TRUE(ids.contains("SingleLineWithTrailingSpace"));
    EXPECT_TRUE(ids.contains("SingleLineWithoutTrailingSpace"));
    EXPECT_TRUE(ids.contains("MultiLineWithTrailingSpace"));
    EXPECT_TRUE(ids.contains("MultiLineWithoutTrailingSpace"));
}

TEST(xml, carriage_return_in_content) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "CRLF.xml");

    const node* structured = root.find("StructuredEmbedded");
    ASSERT_NE(structured, nullptr);
    EXPECT_NE(joined_text(*structured).find("Simple Text"), std::string::npos);
    EXPECT_NE(joined_text(*structured).find("\r\n"), std::string::npos);

    // Content that no child and no reference interrupts stays one slice of the
    // document, so it comes out exactly as it is written there.
    const node* embedded = root.find("Embedded");
    ASSERT_NE(embedded, nullptr);
    EXPECT_EQ(joined_text(*embedded), "Simple Text");
}
