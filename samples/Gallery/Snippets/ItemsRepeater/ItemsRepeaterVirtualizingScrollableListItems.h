// Five hundred numbers; the repeater is given the number of each as its item.
auto const numbered = [](Object const& item) -> UIElement {
    int const number = static_cast<int>(intOf(item));
    if (number % 2 == 0) {
        return Border {background = brushes.SystemControl.Background.Chrome.Medium,
                       TextBlock {hAlign.center, vAlign.center, gallery::numberText(number)}};
    }
    return Border {background = brushes.SystemControl.Background.Accent,
                   TextBlock {foreground = brushes.SystemControl.Foreground.Chrome.White, hAlign.center, vAlign.center,
                              gallery::numberText(number)}};
};

// ActivityFeedLayout of the original: a virtualizing layout written as two functions, which is
// all the repeater needs of it.
auto const feed = gallery::activityFeedLayout({.columnSpacing = 12, .rowSpacing = 12, .minItemSize = {80, 108}});
auto const grid = UniformGridLayout {minItemWidth = 108, minItemHeight = 108, minRowSpacing = 12, minColumnSpacing = 12};

auto const repeater = ItemsRepeater {Margin {0, 0, 12, 0},
                                     hAlign.stretch,
                                     layout = feed,
                                     itemTemplate = numbered,
                                     itemsSource = indexList(500)};

auto example = ScrollViewer {height = 400, Padding {0, 0, 16, 0}, isVerticalScrollChainingEnabled = false, content = repeater};

auto options = StackPanel {
    spacing = 12,
    RadioButtons {
        RadioButton {content = u"Uniform grid"},
        RadioButton {content = u"Custom virtualizing layout"},
        selectedIndex = 1,
        onSelectionChanged = [repeater, feed, grid](RadioButtons const& self) {
            if (self.selectedIndex() == 0) {
                repeater.layout(grid);
            } else if (self.selectedIndex() == 1) {
                repeater.layout(feed);
            }
        },
    },
};