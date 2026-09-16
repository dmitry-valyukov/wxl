// gtest first: it brings standard headers with it, and <parser.h> imports
// wxl.core, after which MSVC rejects a standard header it has not seen yet.
#include <gtest/gtest.h>

#include "config.h"

import wxl.xml;

using namespace wxl::xml;

TEST(xml, byte_order_mark) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "BOM.xml");

    EXPECT_EQ(root.name(), "test");
}

TEST(xml, byte_order_mark_in_a_view) {
    document p;
    const node& root = p.load("\xEF\xBB\xBF<?xml version=\"1.0\"?><test/>");

    EXPECT_EQ(root.name(), "test");

    // Dropped rather than skipped over, so it shifts nothing it precedes.
    EXPECT_EQ(root.line(), 1);
    EXPECT_EQ(root.column(), 22);
}

TEST(xml, utf16_is_refused) {
    document p;

    EXPECT_THROW(p.load(std::string("\xFF\xFE<\0a\0/\0>\0", 10)), exception);
    EXPECT_THROW(p.load(std::string("\xFE\xFF\0<\0a\0/\0>", 10)), exception);
}
