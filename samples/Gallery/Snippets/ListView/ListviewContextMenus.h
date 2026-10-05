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
struct Model {
    ListView list {width = 400, height = 400, horizontalAlignment = HorizontalAlignment::Left,
                   borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1};
};
auto const model = gallery::hold<Model>();

// A menu on each contact, which takes the contact out of the list. The item the menu belongs to is the one
// the template made it for: its position goes into the handler, and the list finds the item by it.
model->list.itemTemplate([people, contactOf, weak = std::weak_ptr<Model>(model)](Object const& item) {
    auto const row = contactOf(item);
    row.contextFlyout(MenuFlyout {
        MenuFlyoutItem {
            text = u"Delete",
            onClick = [weak, item](auto&&...) {
                auto const model = weak.lock();
                if (!model) return;
                auto const items = model->list.items();
                for (uint32_t at = 0; at < items.size(); ++at) {
                    if (intOf(items.getAt(at)) == intOf(item)) {
                        items.removeAt(at);
                        break;
                    }
                }
            },
        },
    });
    return row;
});
model->list.itemsSource(indexList(static_cast<int64_t>(people->size())));

auto example = model->list;