// Golden tests of the parser: HTML in, the serialized tree out. The
// serialization is canonical HTML-ish markup, so a golden reads as what the
// parse understood -- synonyms folded, whitespace collapsed, recovery done.

#include <gtest/gtest.h>

#include <string>
#include <string_view>

import wxl.html;
import wxl.core;

using namespace wxl::html;

namespace {

// The whole tree as UTF-8, so a failing golden prints readably. The
// serialization is the library's own -- the same canonical markup the
// sibling markups (wxl.bb, wxl.rsdn) golden-test against.
std::string parsed(std::wstring_view input) {
    const document doc = parse(input);
    const std::wstring wide = serialized(doc.root());
    return std::string(wxl::core::checked(std::wstring_view(wide)).value().to_utf8().chars());
}

TEST(parse, bare_text_stays_bare) {
    EXPECT_EQ(parsed(L"Привет, мир"), "Привет, мир");
}

TEST(parse, inline_formatting_and_synonyms) {
    EXPECT_EQ(parsed(L"<b>ж</b><strong>ж</strong><i>к</i><em>к</em><del>з</del><strike>з</strike>"),
              "<b>ж</b><b>ж</b><i>к</i><i>к</i><s>з</s><s>з</s>");
}

TEST(parse, tag_names_are_case_insensitive) {
    EXPECT_EQ(parsed(L"<B>ж</B><Div>б</Div>"), "<b>ж</b><div>б</div>");
}

TEST(parse, whitespace_collapses) {
    EXPECT_EQ(parsed(L"a  \n\t b"), "a b");
}

TEST(parse, block_edges_are_trimmed) {
    EXPECT_EQ(parsed(L"<p>  a  </p>"), "<p>a</p>");
    EXPECT_EQ(parsed(L"<p>a</p> \n <p>b</p>"), "<p>a</p><p>b</p>");
}

TEST(parse, space_between_inlines_survives) {
    EXPECT_EQ(parsed(L"<b>a</b> <i>b</i>"), "<b>a</b> <i>b</i>");
    EXPECT_EQ(parsed(L"a <b>b</b>"), "a <b>b</b>");
    EXPECT_EQ(parsed(L"<b>a</b> b"), "<b>a</b> b");
}

TEST(parse, paragraph_closes_itself) {
    EXPECT_EQ(parsed(L"<p>раз<p>два"), "<p>раз</p><p>два</p>");
    EXPECT_EQ(parsed(L"<p>текст<div>блок</div>"), "<p>текст</p><div>блок</div>");
}

TEST(parse, misnested_formatting_reopens) {
    // The example design.md fixes the rule on: first bold, second bold
    // italic, third italic -- the way browsers recover.
    EXPECT_EQ(parsed(L"<b>first<i>second</b>third</i>"),
              "<b>first<i>second</i></b><i>third</i>");
}

TEST(parse, unknown_tag_vanishes_content_stays) {
    EXPECT_EQ(parsed(L"a <unknown attr=\"x\">b</unknown> c"), "a b c");
}

TEST(parse, lone_angle_bracket_is_text) {
    EXPECT_EQ(parsed(L"2 < 3"), "2 &lt; 3");
}

TEST(parse, unpaired_close_is_ignored) {
    EXPECT_EQ(parsed(L"a</em>b"), "ab");
}

TEST(parse, block_tag_closes_open_inlines) {
    EXPECT_EQ(parsed(L"<b>a<p>b"), "<b>a</b><p>b</p>");
}

TEST(parse, entities) {
    EXPECT_EQ(parsed(L"&amp;&lt;&gt; &laquo;a&raquo; &mdash; &#65;&#x416;"),
              "&amp;&lt;&gt; «a» — AЖ");
}

TEST(parse, unknown_entity_stays_literal) {
    EXPECT_EQ(parsed(L"&nosuch; &am"), "&amp;nosuch; &amp;am");
}

TEST(parse, pre_preserves_whitespace) {
    // Spaces stay; a newline becomes its own <br> node, so every text
    // piece is one whole arena string -- the terminated-view promise would
    // not survive a consumer cutting lines out of one big piece.
    EXPECT_EQ(parsed(L"<pre>\n  a  b\r\nc\n</pre>"), "<pre>  a  b<br>c<br></pre>");
}

TEST(parse, attributes_kept_and_filtered) {
    EXPECT_EQ(parsed(L"<a href=\"x\" onclick=\"evil()\">t</a>"), "<a href=\"x\">t</a>");
    EXPECT_EQ(parsed(L"<img src='y' width=24 height=24 alt=\"z\">"),
              "<img src=\"y\" width=\"24\" height=\"24\" alt=\"z\">");
    EXPECT_EQ(parsed(L"<font color=\"#ff0000\" size=\"+1\" face=\"Georgia\">x</font>"),
              "<font color=\"#ff0000\" size=\"+1\" face=\"Georgia\">x</font>");
    EXPECT_EQ(parsed(L"<div style=\"quote\">x</div>"), "<div style=\"quote\">x</div>");
}

TEST(parse, list_items_close_each_other) {
    EXPECT_EQ(parsed(L"<ul><li>a<li>b</ul>"), "<ul><li>a</li><li>b</li></ul>");
}

TEST(parse, line_break_drops_surrounding_spaces) {
    EXPECT_EQ(parsed(L"строка<br>вторая"), "строка<br>вторая");
    EXPECT_EQ(parsed(L"a <br> b"), "a<br>b");
}

TEST(parse, comments_and_declarations_vanish) {
    EXPECT_EQ(parsed(L"a<!-- x -->b"), "ab");
    EXPECT_EQ(parsed(L"<!DOCTYPE html>a<?pi?>b"), "ab");
}

TEST(parse, utf8_door_matches_wide) {
    const document doc = parse(wxl::core::u8_view(u8"«текст» <b>жирный</b>"));
    const std::wstring wide = serialized(doc.root());
    EXPECT_EQ(std::string(wxl::core::checked(std::wstring_view(wide)).value().to_utf8().chars()),
              "«текст» <b>жирный</b>");
}

// The tree's promise to the WinRT side: a zero right after every view it
// hands out -- the param::hstring contract -- so text crosses into COM as
// a string reference with no copy at the call. One wchar_t per string in
// the arena buys it.
void expect_terminated(const node& element) {
    if (element.is_text()) {
        ASSERT_NE(element.value().data(), nullptr);
        EXPECT_EQ(element.value().data()[element.value().size()], L'\0');
    }
    for (const attribute_t& attr : element.attributes()) {
        ASSERT_NE(attr.value().data(), nullptr);
        EXPECT_EQ(attr.value().data()[attr.value().size()], L'\0');
    }
    for (const node& child : element.children()) expect_terminated(child);
}

TEST(parse, views_are_null_terminated) {
    const document doc =
        parse(L"<p>вода &amp; лёд <a href=\"x\">ссылка</a></p><img src='y' alt=''>");
    expect_terminated(doc.root());
}

TEST(parse, nested_link_closes_the_open_one) {
    // Links do not nest, in HTML or here: a new <a> closes the open one,
    // the way browsers recover. The pending space lands *between* the
    // links: the close happens before it materializes.
    EXPECT_EQ(parsed(L"<a href=\"x\">one <a href=\"y\">two</a>"),
              "<a href=\"x\">one</a> <a href=\"y\">two</a>");
}

TEST(parse, nul_reference_stays_literal) {
    // &#0; would embed a NUL inside a text piece -- legal for a counted
    // string, but a trap for every consumer that trusts the terminator.
    EXPECT_EQ(parsed(L"a&#0;b"), "a&amp;#0;b");
}

TEST(parse, recoveries_are_recorded) {
    const document doc =
        parse(L"<unknown>a</unknown> b</em> <b>c<i>d</b>e</i> &nosuch; &#x110000;");

    const std::span<const parse_error> errors = doc.errors();
    ASSERT_EQ(errors.size(), 5u);

    EXPECT_EQ(errors[0].code, error_t::unknown_tag);
    EXPECT_EQ(errors[0].detail, L"unknown");
    EXPECT_EQ(errors[1].code, error_t::unpaired_close);
    EXPECT_EQ(errors[1].detail, L"i");  // synonyms are folded: </em> is </i>
    EXPECT_EQ(errors[2].code, error_t::misnested_tags);
    EXPECT_EQ(errors[2].detail, L"b");
    EXPECT_EQ(errors[3].code, error_t::bad_entity);
    EXPECT_EQ(errors[3].detail, L"nosuch");
    EXPECT_EQ(errors[4].code, error_t::bad_entity);
    EXPECT_EQ(errors[4].detail, L"#x110000");

    // The details carry the same terminated-view promise as the tree.
    for (const parse_error& error : errors) {
        ASSERT_NE(error.detail.data(), nullptr);
        EXPECT_EQ(error.detail.data()[error.detail.size()], L'\0');
    }
}

TEST(parse, clean_markup_records_nothing) {
    const document doc = parse(L"<p>всё <b>хорошо</b> &amp; чисто</p>");
    EXPECT_TRUE(doc.errors().empty());

    // A bare ampersand in prose is not a broken entity.
    EXPECT_TRUE(parse(L"Tom & Jerry").errors().empty());
}

TEST(parse, tree_owes_the_input_nothing) {
    std::wstring input = L"<p>живёт <b>дольше</b> входа</p>";
    document doc = parse(input);
    input.assign(input.size(), L'#');  // scribble over the source

    const std::wstring wide = serialized(doc.root());
    EXPECT_EQ(std::string(wxl::core::checked(std::wstring_view(wide)).value().to_utf8().chars()),
              "<p>живёт <b>дольше</b> входа</p>");
}

TEST(parse, deep_nesting_is_walked_not_survived) {
    std::wstring input;
    for (int i = 0; i < 10000; ++i) input += L"<div>";
    input += L"дно";

    const document doc = parse(input);

    // Down the first-child chain by the links the nodes carry -- no
    // recursion here either.
    int depth = 0;
    const node* current = &doc.root();
    while (!current->children().empty()) {
        current = &*current->children().begin();
        ++depth;
    }
    EXPECT_EQ(depth, 10001);  // 10000 divs and the text at the bottom
}

TEST(parse, span_carries_a_named_style) {
    EXPECT_EQ(parsed(L"<span style=\"kw\">if</span> x"), "<span style=\"kw\">if</span> x");
    EXPECT_EQ(parsed(L"<span>plain</span>"), "<span>plain</span>");
    // A formatting inline: reopened after a mismatched close, like <b>.
    EXPECT_EQ(parsed(L"<b>a<span style=\"x\">b</b>c</span>"),
              "<b>a<span style=\"x\">b</span></b><span style=\"x\">c</span>");
}

TEST(parse, ordered_list_keeps_its_numbering) {
    EXPECT_EQ(parsed(L"<ol type=\"a\"><li>x</li></ol>"), "<ol type=\"a\"><li>x</li></ol>");
}

TEST(parse, rule_is_a_block_that_holds_nothing) {
    EXPECT_EQ(parsed(L"a<hr>b"), "a<hr>b");
    EXPECT_EQ(parsed(L"<p>a<hr/>b"), "<p>a</p><hr>b");
}

TEST(parse, table_rows_and_cells) {
    EXPECT_EQ(parsed(L"<table><tr><th>h</th><td>1</td></tr>"
                     L"<tr><td colspan=\"2\" rowspan=\"1\">2</td></tr></table>"),
              "<table><tr><th>h</th><td>1</td></tr>"
              "<tr><td colspan=\"2\" rowspan=\"1\">2</td></tr></table>");
}

TEST(parse, cells_close_each_other_and_rows_close_cells) {
    EXPECT_EQ(parsed(L"<table><tr><td>a<td>b<tr><td>c</table>"),
              "<table><tr><td>a</td><td>b</td></tr><tr><td>c</td></tr></table>");
    // A cell with no row yet opens one.
    EXPECT_EQ(parsed(L"<table><td>a</td></table>"), "<table><tr><td>a</td></tr></table>");
}

TEST(parse, stray_content_in_a_table_gets_a_cell) {
    EXPECT_EQ(parsed(L"<table>x<tr>y</tr></table>"),
              "<table><tr><td>x</td></tr><tr><td>y</td></tr></table>");
    EXPECT_EQ(parsed(L"<table><tr><p>p</p></tr></table>"),
              "<table><tr><td><p>p</p></td></tr></table>");
}

TEST(parse, cells_outside_a_table_are_dropped) {
    EXPECT_EQ(parsed(L"<td>x</td> <tr>y</tr>"), "x y");
}

TEST(parse, details_with_its_summary) {
    EXPECT_EQ(parsed(L"<details><summary>t</summary><p>c</p></details>"),
              "<details><summary>t</summary><p>c</p></details>");
    // The summary closes like a paragraph; elsewhere it is dropped.
    EXPECT_EQ(parsed(L"<details><summary>t<p>c</details>"),
              "<details><summary>t</summary><p>c</p></details>");
    EXPECT_EQ(parsed(L"<summary>x</summary>"), "x");
}

// The two ways to ask an element about an attribute, and the one thing they
// disagree about.
//
// attribute_str() answers with the text, and a missing attribute answers the
// same empty -- because in markup they are one thing: <a href=""> is as much
// a missing link as <a>. attribute() is for the caller to whom the presence
// itself is the question; it hands back the attribute rather than its text,
// in a pointer that cannot be null once it is there.
TEST(node, an_attribute_by_its_text_and_by_its_presence) {
    const document doc = parse(L"<a href=\"here\">x</a><a href=\"\">y</a><a>z</a>");

    auto at = doc.root().children().begin();
    const node& valued = *at++;
    const node& empty = *at++;
    const node& missing = *at;

    ASSERT_EQ(valued.tag(), tag_t::a);
    ASSERT_EQ(empty.tag(), tag_t::a);
    ASSERT_EQ(missing.tag(), tag_t::a);

    EXPECT_EQ(valued.attribute_str(attr_t::href), L"here");
    EXPECT_TRUE(empty.attribute_str(attr_t::href).empty());
    EXPECT_TRUE(missing.attribute_str(attr_t::href).empty());

    EXPECT_TRUE(valued.attribute(attr_t::href).has_value());
    EXPECT_TRUE(empty.attribute(attr_t::href).has_value());
    EXPECT_FALSE(missing.attribute(attr_t::href).has_value());

    // What the pointer leads to is the attribute, and asking it costs nothing
    // over a bare pointer: the empty state is the null the wrapper refuses.
    EXPECT_EQ((*valued.attribute(attr_t::href))->value(), L"here");
    static_assert(sizeof(wxl::core::nullable<const attribute_t>) == sizeof(const attribute_t*));
}

}  // namespace
