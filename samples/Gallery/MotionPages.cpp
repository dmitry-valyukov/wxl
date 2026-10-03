#include "MotionPages.h"
#include "Pages.h"

#include <algorithm>

#include "Catalog.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.h"

using namespace wxl;
using namespace wxl::dsl;

namespace gallery {

namespace {

constexpr char16_t lorem[] =
    u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua. "
    u"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. Duis aute irure "
    u"dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint occaecat cupidatat non "
    u"proident, sunt in culpa qui officia deserunt mollit anim id est laborum.";

Grid tile(int tileRow, int tileColumn, Color color) {
    return Grid {row = tileRow, column = tileColumn, minHeight = 150, Margin {6}, background = color};
}

}  // namespace

SamplePage1 samplePage1() {
    constexpr Color dark = rgb(169, 169, 169);
    constexpr Color light = rgb(211, 211, 211);

    auto source = Grid {row = 1, rowSpan = 2, column = 0, minWidth = 250, minHeight = 150, Margin {5},
                        background = brushes.Accent.FillColor.Default};
    auto root = ScrollViewer {content = Grid {
        columnDefinitions = u"auto,*,*",
        rowDefinitions = u"auto,auto,auto,*",
        tile(1, 1, dark),
        tile(1, 2, light),
        tile(2, 1, light),
        tile(2, 2, dark),
        source,
        Grid {row = 3, columnSpan = 3, Margin {6, 12}, TextBlock {styles.TextBlock.Body, textWrapping = TextWrapping::WrapWholeWords, lorem}},
    }};
    return {root, source};
}

SamplePage2 samplePage2() {
    auto destination = Grid {row = 1, column = 0, width = 150, height = 200, minHeight = 150, Margin {12}, vAlign.top,
                             background = brushes.Accent.FillColor.Default};
    auto contentPanel = StackPanel {
        row = 1,
        column = 1,
        minHeight = 200,
        Margin {12},
        TextBlock {Margin {0, 0, 0, 12}, styles.TextBlock.Title, textWrapping = TextWrapping::WrapWholeWords,
                   u"Lorem ipsum dolor sit amet, consectetur adipiscing elit"},
        TextBlock {textWrapping = TextWrapping::WrapWholeWords, lorem},
    };
    auto root = ScrollViewer {content = Grid {columnDefinitions = u"auto,*", rowDefinitions = u"auto,auto", destination, contentPanel}};
    return {root, destination, contentPanel};
}

DetailedInfoPage detailedInfoPage(CustomDataObject const& object) {
    auto goBack = Button {content = u"Go Back", hAlign.left, vAlign.top};
    auto image = Image {maxHeight = 400, vAlign.top, source = object.imageLocation, stretch = Stretch::Uniform};
    auto coordinated = StackPanel {
        column = 1,
        Margin {20, 0},
        vAlign.top,
        TextBlock {Margin {0, 0, 0, 10}, styles.TextBlock.Subheader, object.title},
        StackPanel {orientation.horizontal, TextBlock {FontWeight {700}, styles.TextBlock.Subtitle, u"Views: "},
                    TextBlock {Margin {5, 0, 0, 0}, styles.TextBlock.Subtitle, object.views}},
        StackPanel {orientation.horizontal, TextBlock {FontWeight {700}, styles.TextBlock.Subtitle, u"Likes: "},
                    TextBlock {Margin {5, 0, 0, 0}, styles.TextBlock.Subtitle, object.likes}},
    };
    auto root = Grid {
        rowDefinitions = u"auto,*",
        Grid {hAlign.stretch, vAlign.stretch, goBack},
        Grid {Margin {20, 52, 20, 20}, columnDefinitions = u"auto,*", image, coordinated},
        Grid {row = 1, Margin {10}, TextBlock {styles.TextBlock.Subtitle, object.description}},
    };
    return {root, goBack, image, coordinated};
}

std::vector<std::u16string> sortedControlTitles() {
    std::vector<std::u16string> titles;
    for (auto const& group : catalog().groups) {
        for (auto const& item : group.items) {
            titles.emplace_back(item.title.begin(), item.title.end());
        }
    }
    std::ranges::sort(titles);
    return titles;
}

}  // namespace gallery
