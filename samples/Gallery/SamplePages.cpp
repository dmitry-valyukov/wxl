// Четыре страницы-образца из SampleSupport/SamplePages оригинала: плитки и абзац
// текста. Окна примеров Windowing кладут их в NavigationView, чтобы было что
// листать под заголовком окна.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr Color darkGray = rgb(0xa9, 0xa9, 0xa9);
constexpr Color lightGray = rgb(0xd3, 0xd3, 0xd3);

Grid tile(Color color, int tileRow, int tileColumn) {
    return Grid {minHeight = 150, Margin {6}, background = color, row = tileRow, column = tileColumn};
}

constexpr char16_t const* loremText() {
    return
        u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et "
        u"dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex "
        u"ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu "
        u"fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        u"mollit anim id est laborum.";
}

TextBlock lorem(int textRow, int span) {
    return TextBlock {row = textRow, columnSpan = span, Margin {6, 12}, textWrapping = TextWrapping::WrapWholeWords, loremText()};
}

Grid accentTile(int rows, int tileColumn = 0) {
    return Grid {
        row = 1,
        rowSpan = rows,
        column = tileColumn,
        minWidth = 250,
        minHeight = 150,
        Margin {5},
        background = brushes.Accent.FillColor.Default,
    };
}

template <typename... Children>
ScrollViewer tilesPage(Children&&... children) {
    return ScrollViewer {
        content = Grid {
            columnDefinitions = u"*,*,4*",
            rowDefinitions = u"auto,auto,auto,auto",
            std::forward<Children>(children)...,
            Grid {row = 3, columnSpan = 3, Margin {5}, TextBlock {textWrapping = TextWrapping::WrapWholeWords, loremText()}},
        },
    };
}

}  // namespace

FrameworkElement gallery::samplePage(int number) {
    switch (number) {
        case 1:
            return ScrollViewer {
                content = Grid {
                    columnDefinitions = u"auto,*,*",
                    rowDefinitions = u"auto,auto,auto,*",
                    tile(darkGray, 1, 1),
                    tile(lightGray, 1, 2),
                    tile(lightGray, 2, 1),
                    tile(darkGray, 2, 2),
                    accentTile(2),
                    lorem(3, 3),
                },
            };
        case 2:
            return ScrollViewer {
                content = Grid {
                    columnDefinitions = u"auto,*",
                    rowDefinitions = u"auto,auto",
                    accentTile(1),
                    tile(lightGray, 1, 1),
                },
            };
        case 3:
            return ScrollViewer {
                content = Grid {
                    columnDefinitions = u"2*,*,*",
                    rowDefinitions = u"auto,auto,auto,*",
                    tile(darkGray, 1, 0),
                    tile(lightGray, 1, 1),
                    tile(darkGray, 1, 2),
                    tile(lightGray, 2, 0),
                    tile(darkGray, 2, 1),
                    tile(lightGray, 2, 2),
                    lorem(3, 3),
                },
            };
        case 5:
            return tilesPage(
                tile(rgb(0xf0, 0xe6, 0x8c), 0, 0), tile(rgb(0xbd, 0xb7, 0x6b), 0, 1),
                Ellipse {column = 2, width = 150, height = 150, fill = rgb(0x8f, 0xbc, 0x8f)},
                Ellipse {row = 1, columnSpan = 2, width = 75, height = 75, fill = rgb(0x3c, 0xb3, 0x71)},
                tile(rgb(0x55, 0x6b, 0x2f), 1, 2));
        case 6:
            return tilesPage(
                Ellipse {width = 20, height = 20, fill = rgb(0x48, 0xd1, 0xcc)},
                Ellipse {column = 1, width = 50, height = 50, fill = rgb(0x46, 0x82, 0xb4)}, tile(rgb(0x87, 0xce, 0xeb), 0, 2),
                Ellipse {row = 1, width = 50, height = 50, fill = rgb(0xb0, 0xe0, 0xe6)},
                Ellipse {row = 1, column = 1, width = 20, height = 20, fill = rgb(0x48, 0xd1, 0xcc)},
                tile(rgb(0x46, 0x82, 0xb4), 1, 2));
        default:
            return ScrollViewer {
                content = Grid {
                    columnDefinitions = u"2*,*,*",
                    rowDefinitions = u"auto",
                    tile(darkGray, 0, 0),
                    tile(lightGray, 0, 1),
                    tile(darkGray, 0, 2),
                },
            };
    }
}

FrameworkElement gallery::sampleSettingsPage() {
    return Grid {TextBlock {hAlign.center, vAlign.center, styles.TextBlock.Title, u"Sample Settings Page"}};
}
