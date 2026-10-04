// Страница AccessibilityColorContrast — AccessibilityColorContrastPage оригинала: два цвета, контраст между
// ними по WCAG и три проверки. InlineColorPicker оригинала — здесь `inlineColorPicker`: образец, ColorPicker
// в выпадающей части и поле с шестнадцатеричной записью.

#include "Pages.h"
#include "Shell.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"

#include <cmath>
#include <format>

using namespace wxl;
using namespace wxl::dsl;

namespace {

// Относительная яркость по WCAG: https://www.w3.org/WAI/GL/wiki/Relative_luminance
double relativeLuminance(Color c) {
    auto const channel = [](uint8_t value) {
        double const srgb = value / 255.0;
        return srgb <= 0.04045 ? srgb / 12.92 : std::pow((srgb + 0.055) / 1.055, 2.4);
    };
    return 0.2126 * channel(c.R) + 0.7152 * channel(c.G) + 0.0722 * channel(c.B);
}

// Контраст двух цветов: https://www.w3.org/WAI/GL/wiki/Contrast_ratio
double contrastRatio(Color first, Color second) {
    double const one = relativeLuminance(first);
    double const two = relativeLuminance(second);
    return (std::max(one, two) + 0.05) / (std::min(one, two) + 0.05);
}

std::u16string hexOf(Color c) {
    auto const text = std::format("#{:02X}{:02X}{:02X}", c.R, c.G, c.B);
    return std::u16string {text.begin(), text.end()};
}

// "#RRGGBB" или "RRGGBB"; всё иное — не цвет.
bool parseHex(std::u16string_view text, Color& out) {
    if (!text.empty() && text.front() == u'#') {
        text.remove_prefix(1);
    }
    if (text.size() != 6) {
        return false;
    }
    uint32_t value = 0;
    for (char16_t const letter : text) {
        uint32_t digit;
        if (letter >= u'0' && letter <= u'9') {
            digit = letter - u'0';
        } else if (letter >= u'a' && letter <= u'f') {
            digit = letter - u'a' + 10;
        } else if (letter >= u'A' && letter <= u'F') {
            digit = letter - u'A' + 10;
        } else {
            return false;
        }
        value = value * 16 + digit;
    }
    out = Color {255, static_cast<uint8_t>(value >> 16), static_cast<uint8_t>(value >> 8), static_cast<uint8_t>(value)};
    return true;
}

SolidColorBrush solid(Color const& value) {
    return SolidColorBrush {color = value};
}

struct Model {
    core::observable<Color> text {rgb(0, 0, 0)};
    core::observable<Color> backdrop {rgb(255, 255, 255)};
    core::observable<core::u16_text> textHex {core::u16_text {u"#000000"}};
    core::observable<core::u16_text> backdropHex {core::u16_text {u"#FFFFFF"}};
    core::observable<core::u16_text> ratioText {core::u16_text {u"21:1"}};
    core::observable<bool> normalPass {true};
    core::observable<bool> largePass {true};
    core::observable<bool> componentsPass {true};

    Model() {
        text.on_change([this](Color const&) noexcept { recalculate(); });
        backdrop.on_change([this](Color const&) noexcept { recalculate(); });
    }

    // Что делает поле шестнадцатеричной записи с цветом и цвет — с полем: запись, которую человек правит, не
    // переписывается за ним, пока он печатает.
    void follow(core::observable<Color>& color, core::observable<core::u16_text>& hex) {
        color.on_change([&hex](Color const& value) noexcept {
            Color current;
            if (!parseHex(hex.get().plain(), current) || !(current == value)) {
                hex.set(core::unicode::repaired(hexOf(value)));
            }
        });
        hex.on_change([&color](core::u16_text const& value) noexcept {
            Color parsed;
            if (parseHex(value.plain(), parsed)) {
                color.set(parsed);
            }
        });
    }

    void recalculate() noexcept {
        double const ratio = contrastRatio(text.get(), backdrop.get());
        auto const rounded = std::format("{}", std::round(ratio * 100) / 100);
        ratioText.set(core::unicode::repaired(std::u16string {rounded.begin(), rounded.end()} + u":1"));
        normalPass.set(ratio >= 4.5);
        largePass.set(ratio >= 3.0);
        componentsPass.set(ratio >= 3.0);
    }
};

FrameworkElement inlineColorPicker(char16_t const* header, core::observable<Color>& chosen, core::observable<core::u16_text>& hex, int at) {
    return Grid {
        column = at,
        rowSpan = 2,
        rowDefinitions = u"auto,auto",
        columnDefinitions = u"auto,*",
        TextBlock {columnSpan = 2, Margin {0, 0, 0, 12}, vAlign.bottom, styles.TextBlock.BodyStrong, header},
        SplitButton {
            row = 1,
            Padding {0},
            vAlign.stretch,
            automationName = header,
            content = Rectangle {minHeight = 30, width = 40, fill = BindOutput {chosen, solid}},
            flyout = Flyout {
                content = ColorPicker {
                    color = Bind {chosen},
                    isMoreButtonVisible = false,
                    isHexInputVisible = false,
                    isColorChannelTextInputVisible = false,
                },
            },
        },
        TextBox {row = 1, column = 1, minWidth = 120, hAlign.stretch, Margin {4, 0, 0, 0}, automationName = header, text = Bind {hex}},
    };
}

constexpr Color passGreen = rgb(0, 100, 0);
constexpr Color failRed = rgb(139, 0, 0);

// Одна проверка: кружок с галочкой или крестом, «Pass» или «Fail» и что проверяется.
FrameworkElement check(core::observable<bool>& passed, int rowNumber, char16_t const* what, char16_t const* requires_, double left) {
    return Grid {
        row = rowNumber,
        columnSpacing = 8.0,
        columnDefinitions = u"auto,auto,*",
        Grid {
            Margin {left, 0, 0, 0},
            Ellipse {width = 30, height = 30, vAlign.center, fill = BindOutput {passed, [](bool ok) { return solid(ok ? passGreen : failRed); }}},
            FontIcon {foreground = colors.white, glyph = BindOutput {passed, [](bool ok) { return std::u16string {ok ? u"" : u""}; }}},
        },
        TextBlock {column = 1, width = 40, vAlign.center, styles.TextBlock.BodyStrong,
                   text = BindOutput {passed, [](bool ok) { return std::u16string {ok ? u"Pass" : u"Fail"}; }}},
        StackPanel {column = 2, Padding {0, 0, 12, 0}, vAlign.center,
                    TextBlock {fontWeight = FontWeight {700}, textWrapping = TextWrapping::WrapWholeWords, what},
                    TextBlock {textWrapping = TextWrapping::WrapWholeWords, requires_}},
    };
}

}  // namespace

FrameworkElement gallery::accessibilityColorContrastPage() {
    auto const model = gallery::hold<Model>();
    model->follow(model->text, model->textHex);
    model->follow(model->backdrop, model->backdropHex);

    auto const textBrush = [](Color const& value) { return solid(value); };
    auto const checker = Grid {
        Padding {8},
        columnSpacing = 8.0,
        rowSpacing = 8.0,
        rowDefinitions = u"auto,auto,auto",
        columnDefinitions = u"auto,auto,auto,*",
        inlineColorPicker(u"Text Color", model->text, model->textHex, 0),
        inlineColorPicker(u"Background Color", model->backdrop, model->backdropHex, 1),
        TextBlock {column = 3, Margin {12, 0, 0, 0}, vAlign.center, styles.TextBlock.BodyStrong, u"Contrast Ratio"},
        TextBlock {row = 1, column = 3, Margin {12, -4, 0, 0}, styles.TextBlock.Subtitle, text = BindOutput {model->ratioText}},
        Grid {
            row = 2,
            columnSpan = 4,
            minHeight = 300,
            Margin {12, 0, 12, 12},
            cornerRadius = CornerRadius {4},
            columnDefinitions = u"*,*",
            Grid {
                Padding {8},
                background = brushes.Control.FillColor.Default,
                rowDefinitions = u"*,*,*",
                check(model->normalPass, 0, u"Regular text", u"Requires at least 4.5:1", 12),
                check(model->largePass, 1, u"Large text (14 pt. bold or 18pt. regular)", u"Requires at least 3:1", 12),
                check(model->componentsPass, 2, u"Graphical objects and UI components", u"Requires at least 3:1", 10),
            },
            Grid {
                column = 1,
                Padding {8},
                background = BindOutput {model->backdrop, textBrush},
                rowDefinitions = u"*,*,*",
                TextBlock {Padding {12, 0, 12, 0}, vAlign.center, textWrapping = TextWrapping::WrapWholeWords,
                           foreground = BindOutput {model->text, textBrush}, u"The quick brown fox jumped over the lazy fox."},
                StackPanel {
                    row = 1,
                    Padding {12, 0, 12, 0},
                    vAlign.center,
                    TextBlock {fontSize = 14, fontWeight = FontWeight {700}, textWrapping = TextWrapping::WrapWholeWords,
                               foreground = BindOutput {model->text, textBrush}, u"The quick brown fox jumped over the lazy fox."},
                    TextBlock {fontSize = 18, textWrapping = TextWrapping::WrapWholeWords, foreground = BindOutput {model->text, textBrush},
                               u"The quick brown fox jumped over the lazy fox."},
                },
                StackPanel {
                    row = 2,
                    Padding {12, 0, 12, 0},
                    vAlign.center,
                    orientation.horizontal,
                    spacing = 8.0,
                    Grid {Rectangle {width = 30, height = 30, radiusX = 4.0, radiusY = 4.0, fill = BindOutput {model->text, textBrush}},
                          FontIcon {foreground = colors.white, glyph = u""}},
                    Grid {Rectangle {width = 50, height = 30, radiusX = 15.0, radiusY = 50.0, fill = BindOutput {model->text, textBrush}},
                          Ellipse {width = 15, height = 15, Margin {0, 0, 5, 0}, hAlign.right, vAlign.center, fill = colors.white}},
                    FontIcon {fontSize = 20, foreground = BindOutput {model->text, textBrush}, glyph = u""},
                },
            },
        },
    };

    return StackPanel {
        spacing = 12.0,
        RichTextBlock {
            Paragraph {
                Run {u"Accessibility is about building experiences that make your Windows application usable by people of all abilities. "
                     u"For more information about designing accessible apps: "},
                Hyperlink {navigateUri = u"https://learn.microsoft.com/windows/apps/design/accessibility/accessibility-overview",
                           Run {u"Accessibility overview"}},
                Run {u"."},
                LineBreak {},
                LineBreak {},
                Run {u"To ensure optimal accessibility and usability, apps should strive to use high-contrast and easy-to-read color "
                     u"combinations for text and its background. Not only will this benefit users with lower visual acuity, but this will "
                     u"also ensure visibility and legibility under a wide range of lighting conditions, screens, and device settings."},
                LineBreak {},
                LineBreak {},
                Run {u"Check out the "},
                Hyperlink {navigateUri = u"https://accessibilityinsights.io/", Run {u"Accessibility Insights"}},
                Run {u" app to help you find and fix accessibility issues in your Windows apps."},
            },
        },
        TextBlock {Margin {0, 20, 0, 0}, styles.TextBlock.Subtitle, u"Color Contrast Checker"},
        TextBlock {Margin {0, 0, 0, 10}, styles.TextBlock.Body, textWrapping = TextWrapping::Wrap,
                   u"Use this tool to calculate the contrast ratio of two colors and measure them against the Web Content Accessibility "
                   u"Guidelines (WCAG)."},
        Border {
            background = brushes.SolidBackgroundFillColor.Base,
            borderBrush = brushes.Card.StrokeColorDefault,
            BorderThickness {1},
            cornerRadius = CornerRadius {8},
            child = checker,
        },
    };
}
