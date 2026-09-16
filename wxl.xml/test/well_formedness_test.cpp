#include <gtest/gtest.h>

#include <ranges>
#include <string>

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

// A document holds exactly one node, and what may stand behind it is what
// may stand before it. Text there would otherwise end the parse where it
// stands and be dropped without a word.
TEST(xml_well_formedness, nothing_may_follow_the_root_element) {
    document p;

    EXPECT_THROW(p.load("<a/><b/>"), parsing_exception);
    EXPECT_THROW(p.load("<a/>text"), parsing_exception);
    EXPECT_THROW(p.load("<a/>]]>"), parsing_exception);

    EXPECT_EQ(p.load("<a/> <!-- done --> <?pi data?> ").name(), "a");
}

TEST(xml_well_formedness, an_attribute_may_not_be_repeated) {
    document p;

    EXPECT_THROW(p.load(R"(<a x="1" x="2"/>)"), parsing_exception);
    EXPECT_THROW(p.load(R"(<a x="1" y="2" x="3"/>)"), parsing_exception);

    // Two tags are two sets of attributes, and each may name x once.
    EXPECT_NO_THROW(p.load(R"(<a x="1"><b x="2"/></a>)"));
}

// Char excludes every ASCII control but tab, return and newline.
TEST(xml_well_formedness, control_bytes_are_refused) {
    document p;

    EXPECT_THROW(p.load(std::string("<a>\x01</a>", 9)), parsing_exception);
    EXPECT_THROW(p.load(std::string("<a>\x1F</a>", 9)), parsing_exception);
    EXPECT_THROW(p.load(std::string("<a x=\"\x01\"/>", 10)), parsing_exception);

    EXPECT_NO_THROW(p.load("<a x=\"\t\">\t\r\n</a>"));
}

// A reference and a CDATA section are content whatever they stand for: a
// `&#x20;` is a space of the text and not the byte of indentation it looks
// like. Which of the two a run of whitespace is, the reader does not judge --
// it hands all of it over as it stands, and the consumer, who knows what the
// elements mean, decides.
TEST(xml_well_formedness, a_reference_is_always_content) {
    document p;

    EXPECT_EQ(joined_text(p.load("<a>&#x20;<b/></a>")), " ");
    EXPECT_EQ(joined_text(p.load("<a><![CDATA[ ]]><b/></a>")), " ");
    EXPECT_EQ(joined_text(p.load("<a>\n  <b/>\n</a>")), "\n  \n");
}

// The grammar is recursive; the parser is not. The elements half-read are kept
// in the parser rather than on the stack of the process -- where the bound is
// reached without warning, cannot be caught, and takes the process with it.
TEST(xml_well_formedness, a_document_that_nests_deeply) {
    constexpr int depth = 100'000;

    std::string source;
    source.reserve(depth * 7 + 4);

    for (int index = 0; index != depth; ++index)
        source += "<a>";

    source += "text";

    for (int index = 0; index != depth; ++index)
        source += "</a>";

    document p;
    const node& root = p.load(std::move(source));
    const node* el = &root;
    int reached = 1;

    // Down the elements only: the text at the bottom is a node too, and
    // counting it would make the chain one level longer than it is.
    while (const node* const child = el->child("a")) {
        el = child;
        ++reached;
    }

    EXPECT_EQ(reached, depth);
    EXPECT_EQ(joined_text(*el), "text");

    // Both walks go by the links a node carries rather than down the process
    // stack, so depth costs them nothing -- neither a frame nor an allocation.
    // Joining from the root is the harder half: it descends all 100 000 levels
    // and climbs back out of them.
    EXPECT_EQ(joined_text(root), "text");
    EXPECT_EQ(root.find("needle"), nullptr);
    EXPECT_EQ(root.find("a"), &root);
}

// Depth first, and the first node in document order -- a claim about the
// order the stack unwinds in, not only about what is found.
TEST(xml_well_formedness, find_goes_depth_first_in_document_order) {
    document p;
    const node& root = p.load("<r><a><b><t id='1'/></b></a><c><t id='2'/></c><t id='3'/></r>");

    const node* found = root.find("t");
    ASSERT_NE(found, nullptr);
    EXPECT_EQ(found->required_attribute("id"), "1");

    EXPECT_EQ(root.find("r"), &root);
    EXPECT_NE(root.find("c"), nullptr);
    EXPECT_EQ(root.find("nothing"), nullptr);

    // A tree wide at the top leaves several levels to come back to, and each
    // has to be resumed where it stopped.
    const node& wide = p.load("<r><a><x/></a><b><y/></b><c><z/><w/></c></r>");

    EXPECT_NE(wide.find("w"), nullptr);
    EXPECT_NE(wide.find("y"), nullptr);
    EXPECT_EQ(wide.find("q"), nullptr);
}
