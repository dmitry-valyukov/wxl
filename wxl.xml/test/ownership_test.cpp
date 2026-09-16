#include <gtest/gtest.h>

#include <ranges>

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

// The tree views the buffer the parser took over, so what the caller was
// holding may go away the moment the parse returns.
TEST(xml_parser, the_tree_outlives_what_it_was_read_from) {
    document p;
    const node* root = nullptr;

    {
        const std::string source = R"(<a id="x&amp;y">text</a>)";
        root = &p.load(std::string(source));
    }

    // Written over the memory the document just released, so that a tree still
    // pointing at it would read this instead of what it expects.
    const std::string scribble(64, '#');

    EXPECT_EQ(root->name(), "a");
    EXPECT_EQ(joined_text(*root), "text");
    EXPECT_EQ(root->required_attribute("id"), "x&y");
}

// Enough nodes to fill several arena blocks, and enough children under one
// node to notice if linking a child ever went looking for the end.
TEST(xml_parser, a_document_of_many_elements) {
    constexpr int children = 5000;

    std::string source = "<root>";

    for (int index = 0; index < children; ++index)
        source += R"(<child id="x"/>)";

    source += "</root>";

    document p;
    const node& root = p.load(std::move(source));

    EXPECT_EQ(std::ranges::distance(root.children()), children);
    EXPECT_EQ(root.child("child", children - 1)->required_attribute("id"), "x");
    EXPECT_EQ(root.child("child", children), nullptr);
}

// Every parse starts from nothing: the arena of the last document dies, and
// its tree goes with it. The next parse builds a new one.
TEST(xml_parser, parsing_again_starts_over) {
    document p;

    EXPECT_EQ(p.load(R"(<a x="1"><b/></a>)").name(), "a");
    EXPECT_EQ(std::ranges::distance(p.root()->children()), 1);

    EXPECT_EQ(p.load("<c/>").name(), "c");
    EXPECT_TRUE(p.root()->children().empty());
    EXPECT_TRUE(p.root()->attributes().empty());
}
