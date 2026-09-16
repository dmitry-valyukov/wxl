#include <gtest/gtest.h>

#include <string>
#include <string_view>
#include <vector>

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

namespace {

/// The text a node holds itself, without what stands under its children --
/// which is how a run interrupted by a reference is seen to have been joined
/// into one value.
std::string own_text(const node& el) {
    std::string joined;

    for (const node& child : el.children())
        if (child.is_text())
            joined += child.value().chars();

    return joined;
}

}  // namespace

// A reference splits the text around it, which is the one case where the tree
// cannot hand out a view into the document and has to own the value instead.
TEST(xml, character_references) {
    const std::string_view xml =
        R"(<?xml version="1.0" encoding="utf-8"?>)"
        R"(<templates ns="http://www.eurexchange.com/enhancedbroadcastsolution/templates/version-&#xD;&#xA;1.0">)"
        R"(</templates>)";

    document p;
    const node& root = p.load(std::string(xml));

    EXPECT_EQ(root.name(), "templates");
    EXPECT_EQ(root.required_attribute("ns"),
              "http://www.eurexchange.com/enhancedbroadcastsolution/templates/version-\r\n1.0");
}

TEST(xml, predefined_entities) {
    document p;
    const node& root = p.load(R"(<t a="&lt;&gt;&amp;&apos;&quot;">&lt;&amp;&gt;</t>)");

    EXPECT_EQ(root.required_attribute("a"), "<>&'\"");
    EXPECT_EQ(joined_text(root), "<&>");
}

// Whatever interrupts the text starts a new piece of it, an element and a
// reference alike: the reader hands the pieces over as they arrive and joins
// nothing, so what a caller sees is exactly where the document put its markup.
TEST(xml, content_the_document_splits) {
    document p;
    const node& root = p.load("<a>one<b/>two&amp;three</a>");

    EXPECT_EQ(joined_text(root), "onetwo&three");

    std::vector<std::string_view> runs;

    for (const node& child : root.children())
        if (child.is_text())
            runs.push_back(child.value().chars());

    EXPECT_EQ(runs, (std::vector<std::string_view>{"one", "two", "&", "three"}));
}

// What a reference stands for is nowhere in the document to point at, so those
// bytes are a block the parser owns: parsing again has to release what the
// previous tree was viewing and hand out fresh blocks.
TEST(xml, parsing_again_releases_the_text_the_last_tree_viewed) {
    document p;

    EXPECT_EQ(joined_text(p.load("<a>x&amp;y</a>")), "x&y");
    EXPECT_EQ(joined_text(p.load("<a>p&amp;q</a>")), "p&q");
    EXPECT_EQ(joined_text(p.load("<a>plain</a>")), "plain");
}

// Every level of a nested document keeps its own text, and a reference inside
// it is a piece like any other -- there is no buffer anywhere that a deeper
// level could disturb.
TEST(xml, nested_content_all_needing_a_join) {
    document p;
    const node& root = p.load("<a>1&amp;2<b>3&amp;4<c>5&amp;6</c>7&amp;8</b>9&amp;0</a>");

    const node* b = root("b");
    ASSERT_NE(b, nullptr);

    // What each level holds itself, its own pieces put together.
    EXPECT_EQ(own_text(root), "1&29&0");
    EXPECT_EQ(own_text(*b), "3&47&8");
    EXPECT_EQ(own_text(*b->child("c")), "5&6");

    // And what stands under a node, pieces and all.
    EXPECT_EQ(joined_text(root), "1&23&45&67&89&0");
    EXPECT_EQ(joined_text(*b), "3&45&67&8");
}

TEST(xml, decimal_and_hexadecimal_references) {
    document p;
    const node& root = p.load("<t>&#65;&#x42;&#67;</t>");

    EXPECT_EQ(joined_text(root), "ABC");
}

// Beyond the BMP a code point takes four bytes, so the reader has to write all
// of them rather than truncate the value into whatever fits one.
TEST(xml, reference_beyond_the_basic_plane) {
    document p;
    const node& root = p.load("<t>&#x1F600;</t>");

    EXPECT_EQ(joined_text(root), "\xF0\x9F\x98\x80");
    EXPECT_EQ(joined_text(root).size(), 4u);
}

// Bytes that are not UTF-8 are refused before the grammar ever looks at them:
// the scanner walks bytes and would read a broken sequence as content.
TEST(xml, content_that_is_not_utf8) {
    document p;

    EXPECT_THROW(p.load("<t>\xC3</t>"), exception);              // truncated
    EXPECT_THROW(p.load("<t>\x80</t>"), exception);              // a continuation on its own
    EXPECT_THROW(p.load("<t>\xC0\xAF</t>"), exception);          // over-long '/'
    EXPECT_THROW(p.load("<t>\xED\xA0\x80</t>"), exception);      // a surrogate
}

// Text outside ASCII passes through untouched: no byte of a multi-byte
// sequence can be mistaken for markup.
TEST(xml, content_outside_ascii) {
    document p;
    const node& root = p.load("<t a=\"привет\">日本語</t>");

    EXPECT_EQ(root.required_attribute("a"), "привет");
    EXPECT_EQ(joined_text(root), "日本語");
}

TEST(xml, reference_to_something_that_is_not_a_character) {
    document p;

    EXPECT_THROW(p.load("<t>&#xD800;</t>"), parsing_exception);
    EXPECT_THROW(p.load("<t>&#x110000;</t>"), parsing_exception);
    EXPECT_THROW(p.load("<t>&nosuchentity;</t>"), parsing_exception);
}
