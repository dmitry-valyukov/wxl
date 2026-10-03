// Плитка цвета, пример цвета и сетка плиток; см. ColorTile.h.

#include "ColorTile.h"

#include "Design.h"
#include "Shell.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"
#include "generated/Windows.ApplicationModel.DataTransfer.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

SystemBackdropElement backdropHost(gallery::ColorBackdrop backdrop) {
    auto host = SystemBackdropElement {columnSpan = 2};
    switch (backdrop) {
        case gallery::ColorBackdrop::Acrylic: host.systemBackdrop(DesktopAcrylicBackdrop {}); break;
        case gallery::ColorBackdrop::Mica: host.systemBackdrop(MicaBackdrop {kind = MicaKind::Base}); break;
        default: host.systemBackdrop(MicaBackdrop {kind = MicaKind::BaseAlt}); break;
    }
    return host;
}

Hyperlink linkTo(char16_t const* title, wchar_t const* id) {
    return Hyperlink {onClick = [id](auto&&...) { gallery::navigate({gallery::Place::Item, id}); }, Run {title}};
}

}  // namespace

gallery::ColorTileParts gallery::makeColorTile(ColorTileInfo const& info) {
    ColorTileParts parts {
        .root = Grid {row = info.row, column = info.column, columnDefinitions = u"*,auto"},
        .body = Grid {Padding {12}, rowSpacing = 6.0, rowDefinitions = u"auto,auto,*,auto,auto", columnDefinitions = u"*,auto"},
        .texts = {},
        .copyIcon = FontIcon {glyph = u"", fontSize = 14},
    };
    auto text = [&](TextBlock const& block) {
        parts.texts.push_back(block);
        return block;
    };
    if (info.backdrop != ColorBackdrop::None) {
        parts.root.children().append(backdropHost(info.backdrop));
    }
    parts.body.minHeight(120);
    parts.body.children().append(
        text(TextBlock {isTextSelectionEnabled = true, styles.TextBlock.BodyStrong, textWrapping = TextWrapping::WrapWholeWords, info.name}));
    auto const explanation = TextBlock {row = 1, Margin {0, -4, 0, 0}, isTextSelectionEnabled = true, opacity = 0.8,
                                        styles.TextBlock.Caption, textWrapping = TextWrapping::WrapWholeWords, info.explanation};
    parts.body.children().append(text(explanation));
    if (!!*info.key) {
        parts.body.children().append(Button {
            column = 1,
            rowSpan = 4,
            hAlign.right,
            vAlign.top,
            Padding {4},
            background = colors.transparent,
            borderBrush = colors.transparent,
            automationName = u"Copy brush name",
            toolTip = u"Copy brush name",
            content = parts.copyIcon,
            onClick = [name = std::u16string {info.key}](auto&&...) {
                auto package = DataPackage {};
                package.setText(name);
                Clipboard::setContent(package);
            },
        });
        parts.body.children().append(text(TextBlock {row = 3, columnSpan = 2, isTextSelectionEnabled = true, styles.TextBlock.Caption,
                                                    textWrapping = TextWrapping::Wrap, info.key}));
    }
    if (info.comment) {
        auto const note = TextBlock {row = 4, columnSpan = 2, styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap,
                                     Run {u"See "}, linkTo(u"SystemBackdrop", L"SystemBackdrops"), Run {u" and "},
                                     linkTo(u"SystemBackdropElement", L"SystemBackdropElement")};
        parts.body.children().append(text(note));
    }
    parts.root.children().append(parts.body);
    if (info.separator) {
        parts.root.children().append(Border {column = 1, width = 1, hAlign.right, vAlign.stretch, borderBrush = brushes.Card.StrokeColorDefault,
                                             BorderThickness {1}});
    }
    return parts;
}

gallery::ColorExampleParts gallery::makeColorExample(char16_t const* exampleTitle, char16_t const* description,
                                                     FrameworkElement const& content) {
    ColorExampleParts parts {
        .root = Grid {Margin {0, 36, 0, 8}, Padding {12}, rowSpacing = 4.0, rowDefinitions = u"auto,auto,auto",
                      borderBrush = brushes.Card.StrokeColorDefault, BorderThickness {1}, cornerRadius = CornerRadius {8}},
        .title = TextBlock {styles.TextBlock.Subtitle, exampleTitle},
        .description = TextBlock {row = 1, opacity = 0.8, styles.TextBlock.Caption, description},
    };
    parts.root.children().append(parts.title);
    parts.root.children().append(parts.description);
    parts.root.children().append(Border {row = 2, Margin {0, 8, 0, 0}, hAlign.center, content});
    return parts;
}

FrameworkElement gallery::tileGrid(int columns, int rows, std::vector<FrameworkElement> tiles) {
    std::u16string columnList;
    for (int i = 0; i < columns; ++i) {
        columnList += i ? u",*" : u"*";
    }
    std::u16string rowList;
    for (int i = 0; i < rows; ++i) {
        rowList += i ? u",auto" : u"auto";
    }
    auto grid = Grid {columnDefinitions = columnList, rowDefinitions = rowList};
    for (auto const& tile : tiles) {
        grid.children().append(tile);
    }
    return Border {
        background = brushes.SolidBackgroundFillColor.Base,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        cornerRadius = CornerRadius {8},
        child = grid,
    };
}
