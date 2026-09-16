#include <gtest/gtest.h>

#include <format>
#include <string>
#include <string_view>

#include "config.h"

import wxl.xml;

#include "joined_text.h"

using namespace wxl::xml;

namespace {

/// Flattens a FIX dialect the way an engine reads it: one line per field,
/// naming the message it belongs to and the repeating group leading to it.
void describe_message(const node& message, const std::string_view version, const std::string_view type,
                      const std::string_view leading_group, std::string& description) {
    for (const auto& child : message.children()) {
        if (child.name() == "Field") {
            const auto tag = child.attribute("tag");

            if (!tag)
                continue;

            const bool required = child.attribute("isRequired") == "true";

            description += std::format("Version={};MsgType={};LeadingGroup={};Tag={};IsRequired={}|", version, type,
                                       leading_group, tag->chars(), required ? "True" : "False");
        } else if (child.name() == "Group") {
            if (const auto instances = child.attribute("numberOfInstancesTag"))
                describe_message(child, version, type, instances->chars(), description);
        }
    }
}

std::string describe_dialect(const node& root) {
    std::string description;

    for (const auto& block : root.children()) {
        if (block.name() != "FIX")
            continue;

        const auto version = block.attribute("version");

        if (!version)
            continue;

        for (const auto& message : block.children()) {
            if (message.name() != "Message")
                continue;

            if (const auto type = message.attribute("type"))
                describe_message(message, std::format("FIX.{}", version->chars()), type->chars(), {},
                                 description);
        }
    }

    return description;
}

}  // namespace

TEST(xml, traverse) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "LavaDialect.xml");

    EXPECT_EQ(describe_dialect(root),
              "Version=FIX.4.2;MsgType=N;LeadingGroup=73;Tag=37;IsRequired=False|"
              "Version=FIX.4.3;MsgType=y;LeadingGroup=146;Tag=64;IsRequired=True|"
              "Version=FIX.4.3;MsgType=W;LeadingGroup=268;Tag=278;IsRequired=False|"
              "Version=FIX.4.3;MsgType=W;LeadingGroup=;Tag=64;IsRequired=False|"
              "Version=FIX.4.3;MsgType=X;LeadingGroup=;Tag=55;IsRequired=False|"
              "Version=FIX.4.3;MsgType=X;LeadingGroup=;Tag=64;IsRequired=False|");
}

TEST(xml, missing_attribute_names_its_element) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "LavaDialect.xml");

    const node* test_element = root("TestElement");
    ASSERT_NE(test_element, nullptr);
    EXPECT_TRUE(test_element->attributes().empty());
    EXPECT_FALSE(test_element->has_attribute("Dummy"));
    EXPECT_NE(joined_text(*test_element).find("to test 'noAttributs'"), std::string::npos);

    try {
        test_element->required_attribute("Dummy");
        FAIL() << "required_attribute has to throw when the attribute is absent";
    } catch (const exception& e) {
        // The place, and not the document: a node knows where in a document it
        // stands, while the name of that document belongs to the parser.
        EXPECT_STREQ(e.what(), "(41,3): the element 'TestElement' has no attribute 'Dummy'");
    }
}

// A lookup that matches nothing answers with a null pointer: there is no
// node standing for "not found", so the caller tests what it got.
TEST(xml, lookups_that_match_nothing) {
    document p;
    const node& root = p.load_file(WXL_XML_TEST_DATA_DIR "LavaDialect.xml");

    EXPECT_EQ(root("NoSuchElement"), nullptr);
    EXPECT_EQ(root("FIX", 7), nullptr);
    EXPECT_NE(root("FIX", 1), nullptr);
    EXPECT_EQ(root.find("NoSuchElement"), nullptr);
}

// By default not a node is built for a comment: what reads a document reads it
// for its markup.
TEST(xml, comments_ignored) {
    document p;
    const node& root = p.load("<a><!-- text --><b/><!-- more --></a>");

    ASSERT_EQ(std::ranges::distance(root.children()), 1);
    EXPECT_EQ(root.children().begin()->name(), "b");
}

// A caller that asks for them gets them as nodes of their own, children like
// any other. The children are an intrusive list, so they are walked, not
// indexed; what a node is, is its type(), not its name.
TEST(xml, comments_are_children) {
    document p(options{.keep_text = false, .keep_comments = true});
    const node& root = p.load("<a><!-- text --><b/></a>");

    ASSERT_EQ(std::ranges::distance(root.children()), 2);

    auto child = root.children().begin();
    EXPECT_EQ(child->type(), node_type::comment);
    EXPECT_EQ(child->value(), " text ");

    ++child;
    EXPECT_EQ(child->name(), "b");
}

// Without text nodes the text is read and dropped rather than joined into
// anything: the reader never copies the bytes between the tags, so a document
// read for its markup pays for none of them.
TEST(xml, text_not_kept_is_text_dropped) {
    document p(options{.keep_text = false});

    const node& plain = p.load("<a>text</a>");

    EXPECT_EQ(plain.value(), "");
    EXPECT_EQ(std::ranges::distance(plain.children()), 0);

    const node& mixed = p.load("<a>one<b/>two</a>");

    EXPECT_EQ(mixed.value(), "");
    ASSERT_EQ(std::ranges::distance(mixed.children()), 1);
    EXPECT_EQ(mixed.children().front()->name(), "b");
}

/// With text nodes on -- the default -- the same text is read by walking the
/// pieces, wherever under the element they stand.
TEST(xml, text_of_a_subtree) {
    document p;

    EXPECT_EQ(joined_text(p.load("<a>text</a>")), "text");
    EXPECT_EQ(joined_text(p.load("<a>one<b>two</b>three</a>")), "onetwothree");
    EXPECT_EQ(joined_text(p.load("<a><b/></a>")), "");
}

/// The pieces come out apart and in document order, and each of them is a view
/// into the document: an element of plain text answers with the very bytes the
/// document holds, so a consumer that does not need one buffer never pays for
/// one.
TEST(xml, text_comes_out_in_pieces) {
    document p;
    const node& root = p.load("<a>one<b>two</b>three</a>");
    const node::text_range pieces = root.text_pieces();

    auto at = pieces.begin();

    ASSERT_NE(at, pieces.end());
    EXPECT_EQ(*at++, "one");
    ASSERT_NE(at, pieces.end());
    EXPECT_EQ(*at++, "two");
    ASSERT_NE(at, pieces.end());
    EXPECT_EQ(*at++, "three");
    EXPECT_EQ(at, pieces.end());

    // A node of text is one piece, its own.
    const node* const two = root.find("b");

    ASSERT_TRUE(two);
    EXPECT_EQ(joined_text(*two->children().front()), "two");
}

TEST(xml, a_single_piece_is_the_document_itself) {
    document p;
    const node& root = p.load("<a>plain</a>");
    const node* const text = root.children().front();

    ASSERT_TRUE(text);
    EXPECT_EQ((*root.text_pieces().begin()).data(), text->value().data());
}

TEST(xml, cdata_is_taken_as_it_stands) {
    document p;
    const node& root = p.load("<a><![CDATA[<not markup> & neither is this]]></a>");

    EXPECT_EQ(joined_text(root), "<not markup> & neither is this");
}
