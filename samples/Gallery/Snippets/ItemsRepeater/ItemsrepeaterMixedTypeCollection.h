// The collection of the original holds integers and strings; here an entry says which it is,
// and the repeater is given the position of each.
struct Entry {
    bool isText;
    int number;
    std::u16string text;
};
auto const entries = std::make_shared<std::vector<Entry>>(std::vector<Entry> {
    {false, 64, {}},
    {true, 0, u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna aliqua."},
    {false, 128, {}},
    {true, 0, u"Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat."},
    {false, 256, {}},
    {true, 0, u"Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur."},
    {false, 512, {}},
    {true, 0, u"Excepteur sint occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum."},
    {false, 1024, {}},
});

// What the DataTemplateSelector of the original does: the template follows the type of the item.
auto const stringOrInt = [entries](Object const& item) -> UIElement {
    auto const& entry = (*entries)[static_cast<size_t>(intOf(item))];
    if (entry.isText) {
        return Grid {Margin {10},
                     background = brushes.SystemControl.Background.Accent,
                     TextBlock {Padding {10}, foreground = brushes.SystemControl.Foreground.Chrome.White, hAlign.center, vAlign.center,
                                textWrapping = TextWrapping::Wrap, entry.text}};
    }
    return Grid {Margin {10},
                 background = brushes.SystemControl.Background.Chrome.Medium,
                 TextBlock {Padding {10}, styles.TextBlock.Header, hAlign.center, vAlign.center, gallery::numberText(entry.number)}};
};

auto example = StackPanel {
    TextBlock {text = u"This is an ItemsRepeater that displays both integer and string items. It uses a DataTemplateSelector to choose "
                      u"the correct layout for each of its items.",
               textWrapping = TextWrapping::Wrap},
    ItemsRepeater {Margin {0, 0, 12, 0},
                   hAlign.stretch,
                   layout = UniformGridLayout {minItemHeight = 200, minItemWidth = 200},
                   itemTemplate = stringOrInt,
                   itemsSource = indexList(static_cast<int64_t>(entries->size()))},
};