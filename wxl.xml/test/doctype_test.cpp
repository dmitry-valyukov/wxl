#include <gtest/gtest.h>

#include "config.h"

import wxl.xml;

using namespace wxl::xml;

TEST(xml, doctype) {
    document p;

    ASSERT_NO_THROW({
        const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "DOCTYPE.xml");
        EXPECT_EQ(root.name(), "connectivityConfiguration");
    });
}

TEST(xml, unterminated_doctype) {
    document p;

    EXPECT_THROW(p.load("<!DOCTYPE broken"), parsing_exception);
    EXPECT_THROW(p.load("<!DOCTYPE a [<!ENTITY x 'y'><a/>"), parsing_exception);
}

// The end of a declaration is not simply the first '>': a quoted identifier
// may hold one, and an internal subset certainly does.
TEST(xml, doctype_with_an_internal_subset) {
    document p;

    EXPECT_EQ(p.load("<!DOCTYPE a [<!ENTITY x 'y'>]><a/>").name(), "a");
    EXPECT_EQ(p.load("<!DOCTYPE a [<!ENTITY x ']>'>]> <a/>").name(), "a");
    EXPECT_EQ(p.load(R"(<!DOCTYPE a SYSTEM "a>b.dtd"><a/>)").name(), "a");
    EXPECT_EQ(p.load("<!DOCTYPE a [<!-- ] --><!ELEMENT a (#PCDATA)>]><a/>").name(), "a");
}

// Read, not obeyed: this reader has no DTD, so an entity a document declares
// for itself is refused where it is used rather than silently resolved.
TEST(xml, an_entity_a_document_declares_is_still_unknown) {
    document p;

    EXPECT_THROW(p.load("<!DOCTYPE a [<!ENTITY x 'y'>]><a>&x;</a>"), parsing_exception);
}
