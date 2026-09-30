// Плитка контрола и сетка плиток — ControlItemTemplate и GridView оригинала:
// одни и те же плитки на Home, All controls, страницах разделов и выдаче
// поиска.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

// «ms-appx:///Assets/…» оригинала — путь от папки рядом с exe.
std::wstring gallery::assetPath(std::wstring_view imagePath) {
    constexpr std::wstring_view scheme = L"ms-appx:///";
    if (imagePath.starts_with(scheme)) {
        imagePath.remove_prefix(scheme.size());
    }
    return std::wstring{imagePath};
}

wxl::FrameworkElement gallery::controlTile(ControlInfo const& item) {
    auto const image = gallery::assetPath(item.imagePath);

    auto tile = Grid {
        Grid {
            columnDefinitions = u"auto,*",
            Image {
                width = 32,
                Margin {8, 12, 16, 0},
                vAlign.top,
                source = std::wstring_view{image},
            },
            StackPanel {
                column = 1,
                vAlign.center,
                spacing = 4.0,
                TextBlock {
                    item.title,
                    styles.TextBlock.BodyStrong,
                    textWrapping.noWrap,
                },
                TextBlock {
                    item.subtitle,
                    styles.TextBlock.Caption,
                    foreground = brushes.Text.FillColor.Secondary,
                    textTrimming = TextTrimming::WordEllipsis,
                },
            },
        },
    };

    // Плашка экспериментального контрола вдоль нижнего края плитки.
    if (item.isExperimental) {
        tile.children().append(Border {
            Margin {-8},
            Padding {8, 4},
            vAlign.bottom,
            CornerRadius {0, 0, 7, 7},
            background = brushes.SystemFillColor.CautionBackground,
            TextBlock {
                u"Experimental",
                hAlign.center,
                styles.TextBlock.Caption,
                foreground = brushes.SystemFillColor.Caution,
            },
        });
    }

    // Не перенесённый контрол виден, но не открывается — как в оригинале
    // контрол, не попавший в сборку.
    bool const ported = pageFor(item.uniqueId) != nullptr;
    return Border {
        width = 300,
        height = 96,
        Padding {8},
        hAlign.stretch,
        background = brushes.Control.FillColor.Default,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        CornerRadius {8},
        opacity = ported ? 1.0 : 0.5,
        name = std::wstring_view{item.uniqueId},
        tile,
    };
}

wxl::FrameworkElement gallery::tileGrid(std::span<ControlInfo const* const> items,
                                        wxl::Thickness padding) {
    auto grid = GridView {
        selectionMode = ListViewSelectionMode::None,
        isItemClickEnabled = true,
        Padding {padding},
        onItemClick = [](Object const&, ItemClickEventArgs& args) {
            auto const tile = args.clickedItem().try_as<FrameworkElement>();
            if (!tile) {
                return;
            }
            auto const id = gallery::wide(tile.name());
            if (pageFor(id)) {
                navigate({Place::Item, id});
            }
        },
    };
    for (auto const* item : items) {
        grid.items().append(controlTile(*item));
    }
    return grid;
}

std::wstring gallery::htmlEscape(std::wstring_view text) {
    std::wstring result;
    result.reserve(text.size());
    for (wchar_t const symbol : text) {
        switch (symbol) {
        case L'&':
            result += L"&amp;";
            break;
        case L'<':
            result += L"&lt;";
            break;
        case L'>':
            result += L"&gt;";
            break;
        default:
            result += symbol;
        }
    }
    return result;
}
