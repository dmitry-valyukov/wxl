#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

namespace {

/// The children of a node as a document writes them: text in brackets, an
/// element as its name with its own children behind it.
std::string describe(const node& el) {
    std::string description;

    for (const node& child : el.children()) {
        switch (child.type()) {
        case node_type::text:
            description += '[';
            description += child.value().chars();
            description += ']';
            break;

        case node_type::comment:
            description += "<!>";
            break;

        case node_type::element:
            description += child.name().chars();
            description += '(';
            description += describe(child);
            description += ')';
            break;
        }
    }

    return description;
}

}  // namespace

/// The whole point: what stands between two children is kept where it stands.
/// Joined into one value the paragraph below reads "Before  after", and
/// nothing says the emphasis belongs between them.
TEST(mixed_content, keeps_the_order_text_and_elements_were_written_in) {
    document doc;
    const node& root = doc.load("<p>Before <em>middle</em> after</p>");

    EXPECT_EQ(describe(root), "[Before ]em([middle])[ after]");
    EXPECT_EQ(joined_text(root), "Before middle after");
}

TEST(mixed_content, text_of_an_element_without_children_is_a_node_too) {
    document doc;
    const node& root = doc.load("<title>Anna Karenina</title>");

    ASSERT_EQ(std::ranges::distance(root.children()), 1);
    EXPECT_EQ(root.children().begin()->type(), node_type::text);

    // One place to read text from, rather than two that disagree.
    EXPECT_TRUE(root.value().empty());
    EXPECT_EQ(joined_text(root), "Anna Karenina");
}

/// A reference is a piece of its own: what it stands for is not in the
/// document to point at, so it cannot be part of the slice around it, and the
/// reader does not copy the three of them together to pretend otherwise.
TEST(mixed_content, a_reference_is_a_piece_of_its_own) {
    document doc;
    const node& root = doc.load("<p>Tom &amp; Jerry <em>again</em></p>");

    EXPECT_EQ(describe(root), "[Tom ][&][ Jerry ]em([again])");
}

TEST(mixed_content, cdata_is_a_piece_of_its_own) {
    document doc;
    const node& root = doc.load("<p>a<![CDATA[<b>]]>c<em>d</em></p>");

    EXPECT_EQ(describe(root), "[a][<b>][c]em([d])");
}

/// Whitespace is handed over as it stands, indentation included: telling the
/// space inside a sentence from the one between two paragraphs takes knowing
/// what the elements mean, and the reader does not.
TEST(mixed_content, whitespace_between_children_is_kept) {
    document doc;
    const node& root = doc.load("<section>\n  <p>one</p>\n  <p>two</p>\n</section>");

    EXPECT_EQ(describe(root), "[\n  ]p([one])[\n  ]p([two])[\n]");
}

TEST(mixed_content, nesting_keeps_each_levels_own_text) {
    document doc;
    const node& root = doc.load("<p>a<em>b<strong>c</strong>d</em>e</p>");

    EXPECT_EQ(describe(root), "[a]em([b]strong([c])[d])[e]");
    EXPECT_EQ(joined_text(root), "abcde");
}

/// A text node says where it starts, like every other node -- the run's first
/// byte, not the byte that ended it.
TEST(mixed_content, a_text_node_stands_where_its_run_begins) {
    document doc;
    const node& root = doc.load("<p>one\ntwo <em>three</em></p>");

    const node& text = *root.children().begin();
    ASSERT_EQ(text.type(), node_type::text);
    EXPECT_EQ(text.line(), 1);
    EXPECT_EQ(text.column(), 4);  // right behind "<p>"

    const node* const em = root.child("em");
    ASSERT_NE(em, nullptr);
    EXPECT_EQ(em->line(), 2);
}

TEST(mixed_content, comments_keep_their_place_among_the_text) {
    document doc(options{.keep_comments = true});
    const node& root = doc.load("<p>a<!-- note -->b</p>");

    EXPECT_EQ(describe(root), "[a]<!>[b]");
}

/// Children of one name, without writing the loop and the test inside it.
TEST(mixed_content, children_of_one_name) {
    document doc;
    const node& root = doc.load("<ul>\n <li>one</li>\n <li>two</li>\n <p>no</p>\n</ul>");

    std::string joined;
    for (const node& item : root.children_named("li"))
        joined += joined_text(item) + ";";

    EXPECT_EQ(joined, "one;two;");
}
