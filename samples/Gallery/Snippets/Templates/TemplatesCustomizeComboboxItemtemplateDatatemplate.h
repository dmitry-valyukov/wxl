// A DataTemplate is a function from the item to the element that shows it. A ComboBox takes its items as elements, so the
// function is called once per item, here, and its result is the item (a ListView, an ItemsView and an ItemsRepeater call
// the function themselves, as they make the containers: itemTemplate = [](Object const& item) { ... }).
auto const optionOf = [](char16_t const* label) {
    return ComboBoxItem {content = StackPanel {orientation.horizontal, spacing = 8.0,
                                               Ellipse {width = 8, height = 8, vAlign.center, fill = brushes.Accent.FillColor.Default},
                                               TextBlock {label}}};
};

auto example = ComboBox {
    header = u"Options",
    selectedIndex = 0,
    items[optionOf(u"Option 1"), optionOf(u"Option 2"), optionOf(u"Option 3")],
};
