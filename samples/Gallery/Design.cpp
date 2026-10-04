// Опоры страниц раздела Design; см. Design.h.

#include "Design.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Windows.ApplicationModel.DataTransfer.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

void applyTheme(Object const& sender, std::u16string const& stem) {
    if (auto const image = sender.try_as<Image>()) {
        bool const light = image.actualTheme() == ElementTheme::Light;
        image.source(stem + (light ? u".light.png" : u".dark.png"));
    }
}

}  // namespace

FrameworkElement gallery::copyNameButton(std::u16string name) {
    return Button {
        Padding {4},
        automationName = u"Copy to clipboard",
        toolTip = u"Copy to clipboard",
        content = FontIcon {glyph = u"", fontSize = 14},
        onClick = [name](auto&&...) {
            auto package = DataPackage {};
            package.setText(name);
            Clipboard::setContent(package);
        },
    };
}

FrameworkElement gallery::designColumnTitle(char16_t const* text, int at, double left) {
    return TextBlock {
        column = at,
        Margin {left, 0, 0, 0},
        foreground = brushes.Text.FillColor.Secondary,
        styles.TextBlock.Caption,
        text,
    };
}

Image gallery::themedImage(std::u16string stem, double imageHeight) {
    auto image = Image {height = imageHeight, hAlign.left};
    image.add_onLoaded([stem](Object const& sender, auto&&...) { applyTheme(sender, stem); });
    image.add_onActualThemeChanged([stem](Object const& sender, auto&&...) { applyTheme(sender, stem); });
    return image;
}

FrameworkElement gallery::pinnedImage(std::u16string stem, double canvasWidth, double canvasHeight, std::vector<Pin> pins) {
    auto canvas = Canvas {width = canvasWidth, height = canvasHeight, hAlign.left, gallery::themedImage(stem, canvasHeight)};
    for (auto const& pin : pins) {
        auto const button = Button {
            left = pin.left,
            top = pin.top,
            zIndex = 1,
            Padding {4},
            automationName = u"Show " + pin.title,
            toolTip = pin.tooltip.empty() ? pin.title : pin.tooltip,
            content = FontIcon {glyph = u"", fontSize = 16},
        };
        auto const tip = TeachingTip {title = pin.title, subtitle = pin.subtitle, target = button};
        button.add_onClick([tip](auto&&...) { tip.isOpen(!tip.isOpen()); });
        canvas.children().append(button);
        canvas.children().append(tip);
    }
    return ScrollViewer {
        horizontalScrollBarVisibility = ScrollBarVisibility::Auto,
        horizontalScrollMode = ScrollMode::Auto,
        verticalScrollBarVisibility = ScrollBarVisibility::Hidden,
        content = canvas,
    };
}

Grid gallery::designRow(bool shaded, Thickness margin) {
    auto row = Grid {hAlign.stretch, minHeight = 68, cornerRadius = CornerRadius {4}, Margin {margin}};
    if (shaded) {
        row.background(brushes.Card.BackgroundFillColor.Default);
    }
    return row;
}

FrameworkElement gallery::geometryRow(double radius, char16_t const* value, char16_t const* usage, char16_t const* resource,
                                      bool shaded) {
    auto row = gallery::designRow(shaded);
    row.columnDefinitions(u"148,*,160,auto");
    row.children().append(StackPanel {
        orientation.horizontal,
        spacing = 12.0,
        Border {width = 20, height = 20, Margin {16, 0, 0, 0}, background = brushes.Accent.FillColor.Default,
                cornerRadius = CornerRadius {radius}},
        TextBlock {vAlign.center, value},
    });
    row.children().append(TextBlock {column = 1, maxWidth = 400, vAlign.center, textWrapping = TextWrapping::Wrap, styles.TextBlock.Caption, usage});
    row.children().append(TextBlock {column = 2, vAlign.center, isTextSelectionEnabled = true, fontFamily = u"Consolas", styles.TextBlock.Caption, resource});
    if (std::u16string_view{resource} != u"N/a") {
        row.children().append(Border {column = 3, Margin {4, 2, 8, 0}, gallery::copyNameButton(resource)});
    }
    return row;
}
