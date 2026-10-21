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

}  // namespace

TEST(write_doc, writes_nothing_for_documentation_that_says_nothing) {
    EXPECT_EQ(doc_of({}), "");
    EXPECT_EQ(doc_of({.summary = "  ", .params = {{"value", ""}}}, {"value"}, true), "");
}

// Every character Doxygen reads as a command, a tag, an entity or a link.
TEST(write_doc, escapes_what_doxygen_would_read) {
    EXPECT_EQ(doc_of({.summary = R"(A <b> & "c" #d @e \f $g %h.)"}),
              R"(    /// A \<b\> \& \"c\" \#d \@e \\f \$g \%h.)" "\n");
}

// A @param only for a parameter the wxl declaration has, a @return only for
// a declaration that hands something back.
TEST(write_doc, documents_only_what_the_declaration_has) {
    member_documentation const doc{.summary = "Sets it.",
                                   .params = {{"element", "The element."}, {"value", "The value."},
                                              {"inner", "The inner one."}, {"stop", ""}},
                                   .returns = "Nothing.",
                                   .deprecated = "Use another."};
    EXPECT_EQ(doc_of(doc, {"element", "value", "stop"}),
              "    /// Sets it.\n"
              "    ///\n"
              "    /// @deprecated Use another.\n"
              "    /// @param element The element.\n"
              "    /// @param value The value.\n");
    EXPECT_EQ(doc_of({.returns = "The flyout."}, {}, true), "    /// @return The flyout.\n");
}

// Wrapped at 100 columns counted in characters, never starting a line with
// what Markdown would read as a rule or a list.
TEST(write_doc, wraps_at_a_hundred_columns) {
    // "    ///" and fifteen " слово" are 97 columns.
    std::string words;
    for (int word = 0; word < 15; ++word) {
        words += " слово";
    }
    EXPECT_EQ(doc_of({.summary = words + " fit"}), "    ///" + words + "\n    /// fit\n");
    EXPECT_EQ(doc_of({.summary = words + " ---  tail"}), "    ///" + words + " ---\n    /// tail\n");
    EXPECT_EQ(doc_of({.summary = words + " 12. tail"}), "    ///" + words + " 12.\n    /// tail\n");
}

// A line ending with a backslash would have the compiler splice the next line
// onto it, so one that would is closed by an empty HTML comment -- at the end
// of a paragraph and where it wraps alike. The text is a returns of
// SecondaryTile.TileId.
TEST(write_doc, never_ends_a_line_with_a_backslash) {
    EXPECT_EQ(doc_of({.summary = R"(any of these characters: | > < " / ? * \)"}),
              R"(    /// any of these characters: | \> \< \" / ? * \\<!---->)" "\n");
    EXPECT_EQ(doc_of({.summary = R"(a path C:\dir\)"}), R"(    /// a path C:\\dir\\<!---->)" "\n");

    // "    ///" and fourteen " слово" are 91 columns.
    std::string words;
    for (int word = 0; word < 14; ++word) {
        words += " слово";
    }
    EXPECT_EQ(doc_of({.summary = words + R"( ab\ following)"}),
              "    ///" + words + R"( ab\\<!---->)" "\n    /// following\n");
    EXPECT_EQ(doc_of({.summary = words + R"( \ following)"}),
              "    ///" + words + R"( \\<!---->)" "\n    /// following\n");
}
