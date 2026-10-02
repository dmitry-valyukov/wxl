auto const* people = &gallery::contacts();

// A list long enough to need scrolling: the contacts ten times over, numbered.
size_t const total = people->size() * 10;
auto const list = ListView {
    width = 350,
    height = 300,
    horizontalAlignment = HorizontalAlignment::Left,
    borderBrush = brushes.Control.Strong.StrokeColorDefault,
    borderThickness = 1,
    itemTemplate = [people](Object const& item) {
        auto const position = static_cast<size_t>(intOf(item));
        std::u16string text = core::to_u16(static_cast<int>(position) + 1).plain();
        text += u". ";
        text += (*people)[position % people->size()].name();
        return TextBlock {Margin {0, 5, 0, 5}, text};
    },
    itemsSource = indexList(static_cast<int64_t>(total)),
};

auto const index = NumberBox {header = u"Item index", minimum = 0.0, value = 0.0, spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline};
auto const alignment = ComboBox {header = u"Alignment", ComboBoxItem {content = u"Default"}, ComboBoxItem {content = u"Leading"}, selectedIndex = 0};

auto example = StackPanel {list, Margin {0, 8, 0, 0}};

auto options = StackPanel {
    spacing = 8,
    index,
    alignment,
    Button {
        content = u"Scroll into view",
        onClick = [list, index, alignment](Button const&) {
            auto const at = static_cast<int>(index.value());
            if (at < 0 || at >= static_cast<int>(list.items().size())) return;
            list.scrollIntoView(list.items().getAt(static_cast<uint32_t>(at)),
                                alignment.selectedIndex() == 1 ? ScrollIntoViewAlignment::Leading : ScrollIntoViewAlignment::Default);
        },
    },
};