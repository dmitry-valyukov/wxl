#include <gtest/gtest.h>

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

TEST(xml, xml_declaration) {
    document p;
    const node& root = p.load(R"(<?xml version="1.0" encoding="UTF-8" standalone="no"?> <node/>)");

    EXPECT_EQ(root.name(), "node");
    EXPECT_EQ(p.xml_version(), "1.0");
}

// Every part after the version is optional, and one that is not there must not
// eat the spaces of the one that is.
TEST(xml, xml_declaration_without_encoding) {
    document p;

    EXPECT_EQ(p.load(R"(<?xml version="1.1"?><node/>)").name(), "node");
    EXPECT_EQ(p.xml_version(), "1.1");

    EXPECT_EQ(p.load(R"(<?xml version="1.0" standalone='yes' ?><node/>)").name(), "node");
    EXPECT_EQ(p.load(R"(<?xml version="1.0" encoding='utf-8' ?><node/>)").name(), "node");
}

TEST(xml, document_without_a_declaration) {
    document p;

    EXPECT_EQ(p.load("<!-- just a comment --><node/>").name(), "node");
}

TEST(xml, processing_instructions) {
    document p;

    EXPECT_EQ(p.load("<?xml-stylesheet href='x.css'?><node/>").name(), "node");
    EXPECT_EQ(p.load("<?php echo 'hello'; ?><node/>").name(), "node");
    EXPECT_EQ(p.load("<node><?target data?></node>").name(), "node");
    EXPECT_EQ(p.load("<node/><?after?>").name(), "node");

    // Dropped, not turned into text: an element holding one and nothing else
    // holds nothing.
    EXPECT_EQ(joined_text(p.load("<node><?target?></node>")), "");

    EXPECT_THROW(p.load("<?unterminated <node/>"), parsing_exception);
}

// The declaration answers to `<?xml` exactly -- lower case, with a space
// behind it. `xml-stylesheet` and `xmlfoo` are ordinary instruction targets,
// and taking either for a declaration refuses a document over a version it
// never claimed to have.
TEST(xml, a_declaration_is_not_just_any_instruction) {
    document p;

    EXPECT_EQ(p.load(R"(<?xml version="1.1"?><node/>)").name(), "node");
    EXPECT_EQ(p.xml_version(), "1.1");

    EXPECT_EQ(p.load(R"(<?XML version="1.1"?><node/>)").name(), "node");
    EXPECT_EQ(p.xml_version(), "1.0");

    EXPECT_EQ(p.load(R"(<?xmlfoo version="1.1"?><node/>)").name(), "node");
    EXPECT_EQ(p.xml_version(), "1.0");
}

TEST(xml, broken_documents) {
    document p;

    EXPECT_THROW(p.load(""), parsing_exception);
    EXPECT_THROW(p.load("<node>"), parsing_exception);
    EXPECT_THROW(p.load("<node></other>"), parsing_exception);
    EXPECT_THROW(p.load("<node attribute></node>"), parsing_exception);
}
