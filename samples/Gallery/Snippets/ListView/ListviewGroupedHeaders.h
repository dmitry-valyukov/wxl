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
// The groups of the original (CollectionViewSource over the contacts grouped by the first letter of the last name,
// with a header template) are headers among the items: a header is an item that the template shows as a title.
// What the template reads lives in an object the template shares: it is called when the list shows an item, long after this
// function has returned, and must not look at what only this function holds.
struct Groups {
    std::vector<std::u16string> headers;
    std::vector<int64_t> order;  // a position of a contact, or -1 - the position of a header
};
auto const groups = std::make_shared<Groups>();
{
    std::vector<std::pair<char16_t, size_t>> byLetter;
    for (size_t index = 0; index < people->size(); ++index) {
        byLetter.emplace_back(static_cast<char16_t>(std::towupper(static_cast<wint_t>((*people)[index].lastName[0]))), index);
    }
    std::stable_sort(byLetter.begin(), byLetter.end(), [](auto const& a, auto const& b) { return a.first < b.first; });
    char16_t current = 0;
    for (auto const& [letter, index] : byLetter) {
        if (letter != current) {
            current = letter;
            groups->headers.push_back(std::u16string{letter});
            groups->order.push_back(-static_cast<int64_t>(groups->headers.size()));
        }
        groups->order.push_back(static_cast<int64_t>(index));
    }
}

auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 15}, textWrapping = TextWrapping::Wrap,
               u"The contacts are grouped by the first letter of the last name, a title above each group."},
    ListView {
        width = 400,
        height = 400,
        horizontalAlignment = HorizontalAlignment::Left,
        borderBrush = brushes.Control.Strong.StrokeColorDefault,
        borderThickness = 1,
        itemTemplate = [contactOf, groups](Object const& item) -> UIElement {
            auto const entry = groups->order[static_cast<size_t>(intOf(item))];
            if (entry < 0) {
                return TextBlock {styles.TextBlock.Title, groups->headers[static_cast<size_t>(-entry) - 1]};
            }
            return contactOf(intBox(entry));
        },
        itemsSource = indexList(static_cast<int64_t>(groups->order.size())),
    },
};