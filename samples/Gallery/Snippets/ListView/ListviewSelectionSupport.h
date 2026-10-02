// The ContactListViewTemplate of the original -- an ellipse, a name over a company -- as a function from the
// item (the position of a contact in the application's vector) to the element that shows it.
auto const* people = &gallery::contacts();
auto const contactOf = [people](Object const& item) {
    auto const& contact = (*people)[static_cast<size_t>(intOf(item))];
    return Grid {
        rowDefinitions = u"*,*",
        columnDefinitions = u"auto,*",
        Ellipse {rowSpan = 2, width = 32, height = 32, Margin {6}, horizontalAlignment = HorizontalAlignment::Center,
                 verticalAlignment = VerticalAlignment::Center, fill = brushes.Control.Strong.FillColorDefault},
        TextBlock {column = 1, Margin {12, 6, 0, 0}, styles.TextBlock.Base, contact.name()},
        TextBlock {row = 1, column = 1, Margin {12, 0, 0, 6}, styles.TextBlock.Body, contact.company},
    };
};
auto const list = ListView {
    width = 400,
    height = 400,
    horizontalAlignment = HorizontalAlignment::Left,
    borderBrush = brushes.Control.Strong.StrokeColorDefault,
    borderThickness = 1,
    selectionMode = ListViewSelectionMode::Single,
    itemTemplate = contactOf,
    itemsSource = indexList(static_cast<int64_t>(people->size())),
};

auto example = StackPanel {
    RichTextBlock {
        Margin {0, 0, 0, 15},
        textWrapping = TextWrapping::Wrap,
        Paragraph {Run {u"You can enable four different selection modes on the right."}},
        Paragraph {Bold {Run {u"None"}}, Run {u" disables selection all together."}},
        Paragraph {Bold {Run {u"Single"}}, Run {u" allows for only one item to be selected in the list."}},
        Paragraph {Bold {Run {u"Multiple"}},
                   Run {u" causes checkboxes to appear next to items, so that multiple items can be chosen from the list."}},
        Paragraph {Bold {Run {u"Extended"}},
                   Run {u" allows the user to select multiple items by using Ctrl+Click to select the individual items they want, "
                        u"or Shift+Click to select a range of contiguous items."}},
    },
    list,
};

auto options = ComboBox {
    Margin {0, 12, 0, 0},
    header = u"SelectionMode",
    ComboBoxItem {content = u"None"},
    ComboBoxItem {content = u"Single"},
    ComboBoxItem {content = u"Multiple"},
    ComboBoxItem {content = u"Extended"},
    selectedIndex = 1,
    onSelectionChanged = [list](ComboBox const& self) {
        static constexpr ListViewSelectionMode modes[] = {ListViewSelectionMode::None, ListViewSelectionMode::Single,
                                                          ListViewSelectionMode::Multiple, ListViewSelectionMode::Extended};
        if (self.selectedIndex() >= 0) list.selectionMode(modes[self.selectedIndex()]);
    },
};