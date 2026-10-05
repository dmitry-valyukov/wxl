// Содержимое примеров страницы Color — то, что ColorPageExample оригинала держит внутри.

#include "ColorTile.h"

#include "Pages.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

template <class Brush>
TextBlock aa(Brush const& brush) {
    return TextBlock {fontSize = 42, fontWeight = FontWeight {600}, foreground = brush, u"Aa"};
}

TextBlock aa() {
    return TextBlock {fontSize = 42, fontWeight = FontWeight {600}, u"Aa"};
}

// Образец на поверхности: рамка карточки, скругление окна.
Grid surface(double sampleWidth, double sampleHeight) {
    return Grid {width = sampleWidth, height = sampleHeight, borderBrush = brushes.Card.StrokeColorDefault, BorderThickness {1},
                 cornerRadius = CornerRadius {8}};
}

// Фон образца — материал, как у окна: слева направо подложка, поверх неё слой.
Grid layered(auto const& layer, SystemBackdrop const& material) {
    auto grid = surface(120, 40);
    grid.children().append(SystemBackdropElement {cornerRadius = CornerRadius {8}, systemBackdrop = material});
    grid.children().append(Grid {width = 90, hAlign.right, background = layer, borderBrush = brushes.Card.StrokeColorDefault,
                                 BorderThickness {1, 0, 0, 0}});
    return grid;
}

}  // namespace

FrameworkElement gallery::colorSample(char16_t const* sampleTitle) {
    std::u16string_view const which = sampleTitle;
    if (which == u"Text") {
        return aa();
    }
    if (which == u"Accent Text") {
        return aa(brushes.Accent.TextFillColor.Primary);
    }
    if (which == u"Text On Accent") {
        return aa(brushes.Text.OnAccent.FillColor.Primary);
    }
    if (which == u"Control Fill" || which == u"Control Elevation (gradient strokes)" || which == u"Control Stroke") {
        return Button {content = u"Text"};
    }
    if (which == u"Control Alt Fill") {
        return ToggleSwitch {offContent = u"", onContent = u""};
    }
    if (which == u"Neutral Solid") {
        return Slider {minWidth = 240, maximum = 100, value = 40};
    }
    if (which == u"Neutral Strong") {
        return ScrollBar {width = 200, height = 20, indicatorMode = ScrollingIndicatorMode::MouseIndicator,
                          orientation = Orientation::Horizontal, visibility = Visibility::Visible};
    }
    if (which == u"Subtle Fill") {
        return StackPanel {
            Grid {Padding {8}, TextBlock {u"Rest"}},
            Grid {minWidth = 120, Padding {12}, background = brushes.SubtleFillColor.Secondary, cornerRadius = CornerRadius {4},
                  TextBlock {u"Hover"}},
        };
    }
    if (which == u"Control On Image Fill") {
        return Grid {
            cornerRadius = CornerRadius {4},
            Image {maxHeight = 150, source = u"Assets/SampleMedia/valley.jpg"},
            Border {width = 20, height = 20, Margin {8}, hAlign.right, vAlign.top, background = brushes.Control.OnImageFillColor.Default,
                    borderBrush = brushes.Control.Strong.StrokeColorDefault, BorderThickness {1}, cornerRadius = CornerRadius {4}},
        };
    }
    if (which == u"Accent Fill") {
        return StackPanel {Button {styles.Button.Accent, content = u"Text"}};
    }
    if (which == u"System") {
        return InfoBar {title = u"Title", isClosable = false, isOpen = true,
                        message = u"This is body text. Windows 11 is faster and more intuitive.", severity = InfoBarSeverity::Error};
    }
    if (which == u"Card Stroke") {
        return Grid {width = 60, height = 48, background = brushes.Card.BackgroundFillColor.Default, borderBrush = brushes.Card.StrokeColorDefault,
                     cornerRadius = CornerRadius {4}};
    }
    if (which == u"Control Strong Stroke") {
        return ToggleSwitch {minWidth = 40, maxWidth = 40, offContent = u"", onContent = u""};
    }
    if (which == u"Surface Stroke") {
        return Grid {width = 120, height = 40, background = brushes.Acrylic.BackgroundFillColor.Base,
                     borderBrush = brushes.SurfaceStrokeColor.Default, BorderThickness {1}, cornerRadius = CornerRadius {8}};
    }
    if (which == u"Divider Stroke") {
        return Grid {width = 120, height = 40, background = brushes.Acrylic.BackgroundFillColor.Base,
                     borderBrush = brushes.SurfaceStrokeColor.Default, BorderThickness {1}, cornerRadius = CornerRadius {8},
                     Border {width = 1, hAlign.center, vAlign.stretch, borderBrush = brushes.DividerStrokeColorDefault, BorderThickness {1}}};
    }
    if (which == u"Focus Stroke") {
        return Grid {
            borderBrush = brushes.FocusStrokeColorOuter, BorderThickness {2}, cornerRadius = CornerRadius {10},
            Grid {borderBrush = brushes.FocusStrokeColorInner, BorderThickness {2}, cornerRadius = CornerRadius {9},
                  Grid {width = 120, height = 40, borderBrush = brushes.SurfaceStrokeColor.Default, BorderThickness {1},
                        cornerRadius = CornerRadius {8}, TextBlock {hAlign.center, vAlign.center, u"Text"}}},
        };
    }
    if (which == u"Card Background") {
        return Border {width = 60, height = 30, background = brushes.Card.BackgroundFillColor.Default, borderBrush = brushes.Card.StrokeColorDefault,
                       BorderThickness {1}, cornerRadius = CornerRadius {4}};
    }
    if (which == u"Smoke Background") {
        return Grid {width = 120, height = 40, background = brushes.Card.BackgroundFillColor.Default, borderBrush = brushes.Card.StrokeColorDefault,
                     BorderThickness {1}, cornerRadius = CornerRadius {8}};
    }
    if (which == u"Layer") {
        auto grid = Grid {width = 120, height = 40, background = brushes.Acrylic.BackgroundFillColor.Base, borderBrush = brushes.Card.StrokeColorDefault,
                          BorderThickness {1}, cornerRadius = CornerRadius {8}};
        grid.children().append(Grid {width = 90, hAlign.right, background = brushes.Layer.FillColorDefault,
                                     borderBrush = brushes.Card.StrokeColorDefault, BorderThickness {1, 0, 0, 0}});
        return grid;
    }
    if (which == u"Layer on Acrylic") {
        return layered(brushes.Layer.OnAcrylicFillColorDefault, DesktopAcrylicBackdrop {});
    }
    if (which == u"Layer on Mica Base Alt") {
        return Grid {
            SystemBackdropElement {cornerRadius = CornerRadius {8}, systemBackdrop = MicaBackdrop {kind = MicaKind::BaseAlt}},
            TabViewItem {width = 150, height = 30, Margin {8}, borderBrush = brushes.Control.StrokeColor.Secondary, BorderThickness {1}, header = u"Text"},
        };
    }
    if (which == u"Solid Background") {
        return Border {width = 120, height = 40, background = brushes.SolidBackgroundFillColor.Base, borderBrush = brushes.Card.StrokeColorDefault,
                       BorderThickness {1}, cornerRadius = CornerRadius {4}};
    }
    if (which == u"Mica Background") {
        auto grid = surface(120, 40);
        grid.background(brushes.Acrylic.BackgroundFillColor.Base);
        grid.children().append(SystemBackdropElement {cornerRadius = CornerRadius {8}, systemBackdrop = MicaBackdrop {kind = MicaKind::Base}});
        return grid;
    }
    if (which == u"Acrylic Background") {
        return Grid {width = 120, height = 40, background = brushes.Acrylic.BackgroundFillColor.Base, borderBrush = brushes.Card.StrokeColorDefault,
                     BorderThickness {1}, cornerRadius = CornerRadius {8}};
    }
    if (which == u"Accent Acrylic Background") {
        return Grid {width = 120, height = 40, background = brushes.Accent.Acrylic.BackgroundFillColorBase, borderBrush = brushes.Card.StrokeColorDefault,
                     BorderThickness {1}, cornerRadius = CornerRadius {8}};
    }
    return TextBlock {sampleTitle};
}
