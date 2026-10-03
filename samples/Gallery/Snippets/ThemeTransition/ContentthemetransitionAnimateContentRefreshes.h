auto const list = ListView {itemContainerTransitions[ContentThemeTransition {}]};

// Five items; "Refresh data" puts in their place five others, and the list shows them with the transition.
auto const fill = [list](bool updated) {
    std::vector<std::u16string> items;
    for (int i = 0; i < 5; ++i) {
        items.push_back((updated ? u"Updated content " : u"Item ") + gallery::numberText(i));
    }
    list.itemsSource(stringList(items));
};
fill(false);

auto example = Grid {list};
auto options = StackPanel {Button {content = u"Refresh data", onClick = [fill](auto&&...) { fill(true); }}};