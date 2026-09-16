#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.xml;

using namespace wxl::xml;

namespace {

/// A parser pointed at a fragment rather than at a document.
///
/// This is what the grammar being a class of its own buys: a rule can be
/// driven on three characters and asked what it took and where it stopped,
/// instead of being reached only through a whole well-formed document.
///
/// The text is a literal at every call below, which is what makes it legal:
/// the parser stops at a terminating zero rather than at a length, and a
/// literal has one.
struct fragment {
    explicit fragment(std::string_view text) : reader(text, storage) {}

    arena storage;
    parser reader;
};

}  // namespace

TEST(parser, takes_a_name_and_stops_after_it) {
    fragment f("book:title=");
    std::string_view name;

    EXPECT_TRUE(f.reader.parse_name(name));
    EXPECT_EQ(name, "book:title");
    EXPECT_EQ(f.reader.peek(), '=');
}

TEST(parser, refuses_a_name_starting_with_a_digit_and_does_not_move) {
    fragment f("1book");
    std::string_view name;

    EXPECT_FALSE(f.reader.parse_name(name));
    EXPECT_EQ(f.reader.peek(), '1');
}

TEST(parser, takes_a_run_of_spaces_of_every_kind) {
    fragment f(" \t\r\n<");

    EXPECT_TRUE(f.reader.parse_spaces());
    EXPECT_EQ(f.reader.peek(), '<');

    // A second call has nothing left to take, and says so rather than
    // pretending it matched an empty run.
    EXPECT_FALSE(f.reader.parse_spaces());
}

TEST(parser, reads_numbers_in_both_bases) {
    fragment decimal("1114111;");
    char32_t value = 0;

    EXPECT_TRUE(decimal.reader.parse_number(value));
    EXPECT_EQ(value, 0x10FFFFu);
    EXPECT_EQ(decimal.reader.peek(), ';');

    fragment hex("d83D;");
    value = 0;

    EXPECT_TRUE(hex.reader.parse_hex_number(value));
    EXPECT_EQ(value, 0xD83Du);
}

TEST(parser, an_equals_sign_may_be_spaced_on_both_sides) {
    fragment f("  =\t\"value\"");

    EXPECT_TRUE(f.reader.parse_eq());
    EXPECT_EQ(f.reader.peek(), '"');

    fragment without("value");

    EXPECT_FALSE(without.reader.parse_eq());
    EXPECT_EQ(without.reader.rest(), "value");
}

TEST(parser, matches_a_keyword_ignoring_case_only_where_asked) {
    fragment f("UTF-8");

    EXPECT_FALSE(f.reader.parse_string("utf-8"));
    EXPECT_TRUE(f.reader.parse_string_no_case("utf-8"));
    EXPECT_EQ(f.reader.peek(), '\0');
}

TEST(parser, a_named_reference_answers_with_the_table_and_a_numeric_one_with_the_arena) {
    fragment named("&amp;rest");

    EXPECT_EQ(named.reader.parse_reference_text(), "&");
    EXPECT_EQ(named.reader.rest(), "rest");

    // Two bytes in UTF-8, so this one is built and kept in the arena rather
    // than pointed at anywhere in the fragment.
    fragment numeric("&#x44F;");

    EXPECT_EQ(numeric.reader.parse_reference_text(), "\xD1\x8F");

    fragment decimal("&#65;");

    EXPECT_EQ(decimal.reader.parse_reference_text(), "A");
}

TEST(parser, an_undeclared_entity_is_a_broken_document) {
    fragment f("&nbsp;");

    EXPECT_THROW((void)f.reader.parse_reference_text(), parsing_exception);
}

TEST(parser, an_attribute_value_comes_out_whole_across_a_reference) {
    // The one thing the grammar has to join: attribute::value() is a single
    // view, and what `&amp;` stands for appears nowhere in the document.
    fragment f(R"("a &amp; b" rest)");
    std::string_view value;

    EXPECT_TRUE(f.reader.parse_attribute_value(value));
    EXPECT_EQ(value, "a & b");

    // Uninterrupted, the value is the document itself and nothing is copied.
    fragment plain(R"('plain')");

    EXPECT_TRUE(plain.reader.parse_attribute_value(value));
    EXPECT_EQ(value, "plain");
    EXPECT_EQ(value.data(), plain.reader.rest().data() - value.size() - 1);
}

TEST(parser, comments_and_cdata_hand_out_what_stands_between_the_markers) {
    fragment comment("<!-- a note --><next/>");
    std::string_view text;

    EXPECT_TRUE(comment.reader.parse_comment(text));
    EXPECT_EQ(text, " a note ");
    EXPECT_EQ(comment.reader.rest(), "<next/>");

    fragment cdata("<![CDATA[<not markup> & co]]>tail");

    EXPECT_TRUE(cdata.reader.parse_cdata(text));
    EXPECT_EQ(text, "<not markup> & co");
    EXPECT_EQ(cdata.reader.rest(), "tail");
}

TEST(parser, a_rule_that_did_not_match_leaves_the_parser_where_it_found_it) {
    fragment f("<!DOCTYPE html>");
    std::string_view text;

    EXPECT_FALSE(f.reader.parse_cdata(text));
    EXPECT_FALSE(f.reader.parse_comment(text));
    EXPECT_EQ(f.reader.rest(), "<!DOCTYPE html>");
}

TEST(parser, parses_a_whole_document_the_way_the_document_class_does) {
    fragment f(R"(<?xml version="1.1"?><root a="1"><child/></root>)");

    const node& root = f.reader.parse();

    EXPECT_EQ(root.name(), "root");
    EXPECT_EQ(f.reader.xml_version(), "1.1");
    EXPECT_EQ(root.attributes().size(), 1u);
    EXPECT_NE(root.find("child"), nullptr);
}
