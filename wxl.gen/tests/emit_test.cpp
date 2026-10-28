#include <gtest/gtest.h>

#include "wxl.gen.h"

// The leading run of capitals is lowered as one word; when a lowercase letter
// follows the run, its last capital already starts the next word.
TEST(member_name, lowers_the_leading_capitals) {
    EXPECT_EQ(gen::member_name("Content"), "content");
    EXPECT_EQ(gen::member_name("IsChecked"), "isChecked");
    EXPECT_EQ(gen::member_name("UIElement"), "uiElement");
    EXPECT_EQ(gen::member_name("ID"), "id");
    EXPECT_EQ(gen::member_name("X"), "x");
}

TEST(member_name, escapes_a_keyword_with_a_trailing_underscore) {
    EXPECT_EQ(gen::member_name("Template"), "template_");
    EXPECT_EQ(gen::member_name("Default"), "default_");
    EXPECT_EQ(gen::member_name("Auto"), "auto_");
}

// `xor` is an operator to the language, not a name: CanvasComposite.Xor.
TEST(member_name, escapes_an_alternative_token_with_a_trailing_underscore) {
    EXPECT_EQ(gen::member_name("Xor"), "xor_");
    EXPECT_EQ(gen::member_name("Not"), "not_");
    EXPECT_EQ(gen::member_name("Protected"), "protected_");
}

TEST(interface_field_name, drops_the_interface_prefix) {
    EXPECT_EQ(gen::interface_field_name("IButtonBase"), "buttonBase_");
    EXPECT_EQ(gen::interface_field_name("IUIElement"), "uiElement_");
    // Not a prefix when the next letter is lowercase.
    EXPECT_EQ(gen::interface_field_name("Icon"), "icon_");
}

TEST(winrt_namespace, spells_the_metadata_namespace_in_cpp) {
    EXPECT_EQ(gen::winrt_namespace("Microsoft.UI.Xaml"), "winrt::Microsoft::UI::Xaml");
}

namespace {

std::string doc_of(member_documentation const& doc, std::vector<std::string_view> const& params = {},
                   bool has_result = false) {
    std::ostringstream out;
    gen::write_doc(out, "    ", doc, params, has_result);
    return out.str();
}

// A piece of text, and a description of text alone, as the reader gives one
// without markup.
doc_run piece(std::string_view text) {
    return {.text = std::string{text}};
}

doc_text plain(std::string_view text) {
    return {piece(text)};
}

// An element of the markup holding `runs`.
doc_run holding(doc_run::kind_t kind, doc_text runs = {}) {
    return {.kind = kind, .runs = std::move(runs)};
}

// Bold or italic written with `mark`.
doc_run emphasis(std::string_view mark, doc_text runs) {
    auto const kind = mark.size() == 2 ? doc_run::kind_t::bold : doc_run::kind_t::italic;
    return {.kind = kind, .text = std::string{mark}, .runs = std::move(runs)};
}

// A code span as the reader gives one: as written, its backticks too.
doc_run code(std::string_view span) {
    return {.kind = doc_run::kind_t::code, .text = std::string{span}};
}

doc_run image(std::string_view url, std::string_view alt) {
    return {.kind = doc_run::kind_t::image, .text = std::string{alt}, .url = std::string{url}};
}

doc_run definition(std::string_view term, std::string_view what) {
    return {.kind = doc_run::kind_t::definition, .text = std::string{term}, .runs = plain(what)};
}

}  // namespace

TEST(write_doc, writes_nothing_for_documentation_that_says_nothing) {
    EXPECT_EQ(doc_of({}), "");
    EXPECT_EQ(doc_of({.summary = plain("  "), .params = {{"value", {}}}}, {"value"}, true), "");
}

// Every character Doxygen reads as a command, a tag, an entity or a link.
TEST(write_doc, escapes_what_doxygen_would_read) {
    EXPECT_EQ(doc_of({.summary = plain(R"(A <b> & "c" #d @e \f $g %h.)")}),
              R"(    /// A \<b\> \& \"c\" \#d \@e \\f \$g \%h.)" "\n");
}

// A @param only for a parameter the wxl declaration has, a @return only for
// a declaration that hands something back.
TEST(write_doc, documents_only_what_the_declaration_has) {
    member_documentation const doc{.summary = plain("Sets it."),
                                   .params = {{"element", plain("The element.")}, {"value", plain("The value.")},
                                              {"inner", plain("The inner one.")}, {"token", {}}},
                                   .returns = plain("Nothing."),
                                   .deprecated = plain("Use another.")};
    EXPECT_EQ(doc_of(doc, {"element", "value", "token"}),
              "    /// Sets it.\n"
              "    ///\n"
              "    /// @deprecated Use another.\n"
              "    /// @param element The element.\n"
              "    /// @param value The value.\n");
    EXPECT_EQ(doc_of({.returns = plain("The flyout.")}, {}, true), "    /// @return The flyout.\n");
}

// Wrapped at 100 columns counted in characters, never starting a line with
// what Doxygen would read as a block marker: a rule, a list item, the dot that
// ends a list, the fence of a code block.
TEST(write_doc, wraps_at_a_hundred_columns) {
    // "    ///" and fifteen " слово" are 97 columns.
    std::string words;
    for (int word = 0; word < 15; ++word) {
        words += " слово";
    }
    EXPECT_EQ(doc_of({.summary = plain(words + " fit")}), "    ///" + words + "\n    /// fit\n");
    EXPECT_EQ(doc_of({.summary = plain(words + " ---  tail")}), "    ///" + words + " ---\n    /// tail\n");
    EXPECT_EQ(doc_of({.summary = plain(words + " 12. tail")}), "    ///" + words + " 12.\n    /// tail\n");
    EXPECT_EQ(doc_of({.summary = plain(words + " ab . tail")}), "    ///" + words + " ab .\n    /// tail\n");
    EXPECT_EQ(doc_of({.summary = plain(words + " ab ```x tail")}), "    ///" + words + " ab ```x\n    /// tail\n");
    EXPECT_EQ(doc_of({.summary = plain(words + " ab ~~~ tail")}), "    ///" + words + " ab ~~~\n    /// tail\n");
}

// A code span is written as it came, since Doxygen escapes what is in one itself,
// and as one word, never broken across lines. But a % before a letter is doubled,
// an odd run of backslashes before the closing backticks gets a space after it,
// a backslash before the span an empty HTML comment, and the span closes after a
// * and opens again before the / after it, and so before the { after a command,
// an empty HTML comment between. In @deprecated's HTML a span is text, escaped as
// HTML.
TEST(write_doc, writes_a_code_span_as_written) {
    using kind = doc_run::kind_t;
    doc_run const capability = code(R"(``<uap4:Capability Name="x" />``)");
    EXPECT_EQ(doc_of({.summary = {piece("Without "), capability, piece(" declared.")}}),
              "    /// Without ``<uap4:Capability Name=\"x\" />`` declared.\n");
    EXPECT_EQ(doc_of({.summary = {code("`%word @c`"), piece(", "), code("`a\\`"), piece(" and "), code("`a */ b`")}}),
              "    /// `%%word @c`, `a\\ ` and `a *`<!---->`/ b`\n");
    EXPECT_EQ(doc_of({.summary = {code("``a\\\\``"), piece(", "), code("```a\\```"), piece(", "), code("``a\\```")}}),
              "    /// ``a\\\\``, ```a\\ ```, ``a\\```\n");
    EXPECT_EQ(doc_of({.summary = {piece("C:\\"), code("`x`")}}), "    /// C:\\\\<!---->`x`\n");
    EXPECT_EQ(doc_of({.summary = {code("`\\c{x`"), piece(", "), code("`@a-b{}`"), piece(", "), code("`x{y`"),
                                  piece(", "), code("`\\{`"), piece(", "), code("`\\1{`"), piece(" and */.")}}),
              "    /// `\\c`<!---->`{x`, `@a-b`<!---->`{}`, `x{y`, `\\{`, `\\1{` and */.\n");
    std::string words;  // "    ///" and fifteen " слово" are 97 columns
    for (int word = 0; word < 15; ++word) {
        words += " слово";
    }
    EXPECT_EQ(doc_of({.summary = {piece(words + " "), code("`a b`")}}), "    ///" + words + "\n    /// `a b`\n");
    doc_run const row = holding(kind::row, {holding(kind::cell, {code("`<x>`")})});
    EXPECT_EQ(doc_of({.deprecated = {piece("Values:"), holding(kind::table, {row})}}),
              "    /// @deprecated Values:\n"
              "    /// @htmlonly<table class=\"doxtable\"> <tr><td>`&lt;x&gt;`</td></tr> </table>@endhtmlonly\n");
}

// A line ending with a backslash would have the compiler splice the next line
// onto it, so one that would is closed by an empty HTML comment -- at the end
// of a paragraph and where it wraps alike. The text is a returns of
// SecondaryTile.TileId.
TEST(write_doc, never_ends_a_line_with_a_backslash) {
    EXPECT_EQ(doc_of({.summary = plain(R"(any of these characters: | > < " / ? * \)")}),
              R"(    /// any of these characters: | \> \< \" / ? * \\<!---->)" "\n");
    EXPECT_EQ(doc_of({.summary = plain(R"(a path C:\dir\)")}), R"(    /// a path C:\\dir\\<!---->)" "\n");

    // "    ///" and fourteen " слово" are 91 columns.
    std::string words;
    for (int word = 0; word < 14; ++word) {
        words += " слово";
    }
    EXPECT_EQ(doc_of({.summary = plain(words + R"( ab\ following)")}),
              "    ///" + words + R"( ab\\<!---->)" "\n    /// following\n");
    EXPECT_EQ(doc_of({.summary = plain(words + R"( \ following)")}),
              "    ///" + words + R"( \\<!---->)" "\n    /// following\n");
}

// Bold and italic go back to the Markdown they came in, with the marks they
// came with, so a description with them is written as it would be if they were
// text.
TEST(write_doc, writes_bold_and_italic_as_markdown) {
    EXPECT_EQ(doc_of({.summary = {piece("A "), emphasis("**", plain("UInt32")), piece(" value, "),
                                  emphasis("_", plain("EaseIn")), piece(" mode, "), emphasis("*", plain("shape")),
                                  piece(" and "), emphasis("__", plain("bold")), piece(".")}}),
              doc_of({.summary = plain("A **UInt32** value, _EaseIn_ mode, *shape* and __bold__.")}));
    EXPECT_EQ(doc_of({.summary = plain("A **UInt32** value, _EaseIn_ mode, *shape* and __bold__.")}),
              "    /// A **UInt32** value, _EaseIn_ mode, *shape* and __bold__.\n");
}

// Emphasis that opens with an image or a line break gets a zero-width
// non-joiner after its mark, since Doxygen opens none right before a tag, and
// the image's line starts with the mark.
TEST(write_doc, opens_emphasis_before_an_image) {
    EXPECT_EQ(doc_of({.summary = {piece("See "), emphasis("**", {image("https://example.com/i.png", "Icon"),
                                                                   piece(" icon")})}}),
              "    /// See\n"
              "    /// **&zwnj;<img src=\"https://example.com/i.png\" alt=\"Icon\"> icon**\n");
    EXPECT_EQ(doc_of({.summary = {emphasis("_", {holding(doc_run::kind_t::line_break), piece("x")})}}),
              "    /// _&zwnj;<br>x_\n");
}

// A superscript, a subscript and a line break are HTML, unescaped, and join the
// words they touch; the text around them is escaped as ever.
TEST(write_doc, writes_scripts_and_line_breaks_as_html) {
    using kind = doc_run::kind_t;
    EXPECT_EQ(doc_of({.summary = {piece("f (t) = t"), holding(kind::superscript, plain("3")), piece(" and x"),
                                  holding(kind::subscript, plain("i")), piece(" of IVector<String>.")}}),
              "    /// f (t) = t<sup>3</sup> and x<sub>i</sub> of IVector\\<String\\>.\n");
    EXPECT_EQ(doc_of({.summary = {emphasis("_", plain("t")), holding(kind::superscript, plain("p"))}}),
              "    /// _t_<sup>p</sup>\n");
    EXPECT_EQ(doc_of({.summary = {piece("One line"), holding(kind::line_break), piece("and the next.")}}),
              "    /// One line<br>and the next.\n");
}

// An image is an <img> with its absolute address and its alt text, and it starts
// a line however short the one before it: Doxygen takes a summary of one line for
// the brief description, and an image after text there would cut it short. The
// text after it goes on on its line.
TEST(write_doc, writes_an_image_starting_a_line) {
    EXPECT_EQ(doc_of({.summary = {piece("E8FB "), image("https://example.com/e8fb.png", "Accept icon")}}),
              "    /// E8FB\n"
              "    /// <img src=\"https://example.com/e8fb.png\" alt=\"Accept icon\">\n");
    EXPECT_EQ(doc_of({.summary = {image("https://example.com/a.png", R"(A & "B" <c>)"), piece(" after.")}}),
              "    /// <img src=\"https://example.com/a.png\" alt=\"A &amp; &quot;B&quot; &lt;c&gt;\"> after.\n");
}

// No line ends in white space where the markup breaks it: the no-break spaces
// before an image go to its line with it, still parting a code from its icon,
// and those before a table go.
TEST(write_doc, ends_no_line_in_white_space) {
    using kind = doc_run::kind_t;
    doc_run const icon = image("https://example.com/e8fb.png", "Accept icon");
    EXPECT_EQ(doc_of({.summary = {piece("E8FB \xC2\xA0\xC2\xA0"), icon}}),
              "    /// E8FB\n"
              "    /// \xC2\xA0\xC2\xA0<img src=\"https://example.com/e8fb.png\" alt=\"Accept icon\">\n");
    doc_run const row = holding(kind::row, {holding(kind::cell, plain("0"))});
    EXPECT_EQ(doc_of({.summary = {piece("Values: \xC2\xA0"), holding(kind::table, {row})}}),
              "    /// Values:\n"
              "    /// <table>\n"
              "    /// <tr><td>0</td></tr>\n"
              "    /// </table>\n");
}

// A list of definitions is a Markdown list, an item a line and its term code;
// a line of a dot closes it when the paragraph goes on after it. Text that then
// starts a line with what would open a block there -- a list item, another dot
// -- gets a zero-width non-joiner in front.
TEST(write_doc, writes_definitions_as_a_markdown_list) {
    doc_run const list = holding(doc_run::kind_t::definitions,
                                 {definition("ms-appx:///", "A path within the package."),
                                  definition("ms-appdata:///local/", "A file.")});
    EXPECT_EQ(doc_of({.returns = {piece("Schemes:"), list}}, {}, true),
              "    /// @return Schemes:\n"
              "    /// - `ms-appx:///` — A path within the package.\n"
              "    /// - `ms-appdata:///local/` — A file.\n");
    EXPECT_EQ(doc_of({.summary = {piece("Schemes:"), list, piece("More.")}}),
              "    /// Schemes:\n"
              "    /// - `ms-appx:///` — A path within the package.\n"
              "    /// - `ms-appdata:///local/` — A file.\n"
              "    /// .\n"
              "    /// More.\n");
    EXPECT_EQ(doc_of({.summary = {piece("Schemes:"), list, piece("- the first, - the second.")}}),
              "    /// Schemes:\n"
              "    /// - `ms-appx:///` — A path within the package.\n"
              "    /// - `ms-appdata:///local/` — A file.\n"
              "    /// .\n"
              "    /// &zwnj;- the first, - the second.\n");
    EXPECT_EQ(doc_of({.summary = {piece("Schemes:"), list, piece(".")}}),
              "    /// Schemes:\n"
              "    /// - `ms-appx:///` — A path within the package.\n"
              "    /// - `ms-appdata:///local/` — A file.\n"
              "    /// .\n"
              "    /// &zwnj;.\n");
}

// A table is an HTML table, a row a line, the text of its cells escaped and
// wrapped as any other; the text after it goes on after </table>.
TEST(write_doc, writes_a_table_as_html) {
    using kind = doc_run::kind_t;
    doc_run const header = holding(kind::row, {holding(kind::header_cell, plain("Value")),
                                               holding(kind::header_cell, plain("Meaning"))});
    doc_run const row = holding(kind::row, {holding(kind::cell, {emphasis("**", plain("0"))}),
                                            holding(kind::cell, plain("No <list>."))});
    doc_run const table = holding(kind::table, {header, row});
    EXPECT_EQ(doc_of({.params = {{"unit", {piece("Values:"), table, piece("And so on.")}}}}, {"unit"}),
              "    /// @param unit Values:\n"
              "    /// <table>\n"
              "    /// <tr><th>Value</th><th>Meaning</th></tr>\n"
              "    /// <tr><td>**0**</td><td>No \\<list\\>.</td></tr>\n"
              "    /// </table> And so on.\n");
}

// In @deprecated a table is HTML Doxygen passes through, on one line between
// @htmlonly and @endhtmlonly, its text escaped as HTML -- a backslash too, lest
// \endhtmlonly in a cell end it -- and its bold as <b>: Doxygen ends the
// deprecated section at a table of its own.
TEST(write_doc, keeps_a_table_inside_deprecated) {
    using kind = doc_run::kind_t;
    doc_run const row = holding(kind::row, {holding(kind::cell, {emphasis("**", plain("0"))}),
                                            holding(kind::cell, plain("No <list> & @more \\endhtmlonly."))});
    EXPECT_EQ(doc_of({.deprecated = {piece("Values:"), holding(kind::table, {row}), piece("And so on.")}}),
              "    /// @deprecated Values:\n"
              "    /// @htmlonly<table class=\"doxtable\"> <tr><td><b>0</b></td>"
              "<td>No &lt;list&gt; &amp; &#64;more &#92;endhtmlonly.</td></tr> </table>@endhtmlonly\n"
              "    /// And so on.\n");
}
