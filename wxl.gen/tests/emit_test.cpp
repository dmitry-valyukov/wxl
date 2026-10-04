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
