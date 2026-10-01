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

TextBlock lorem(int textRow, int span) {
    return TextBlock {
        row = textRow,
        columnSpan = span,
        Margin {6, 12},
        textWrapping = TextWrapping::WrapWholeWords,
        u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et "
        u"dolore magna aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex "
        u"ea commodo consequat. Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu "
        u"fugiat nulla pariatur. Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt "
        u"mollit anim id est laborum.",
    };
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
