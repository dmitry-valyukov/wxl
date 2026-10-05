struct Model {
    ListView list {width = 350, height = 300, horizontalAlignment = HorizontalAlignment::Left,
                   borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1};
    Button restore {content = u"Restore position", isEnabled = false};
    int saved = -1;  // the position of the first item in view when it was saved

    // The scroll position is remembered as an item: the first one in view, which ItemsStackPanel names.
    void save() {
        if (auto const stack = list.itemsPanelRoot().try_as<ItemsStackPanel>()) {
            saved = stack.firstVisibleIndex();
            restore.isEnabled(true);
        }
    }

    void bring() const {
        if (saved >= 0) list.scrollIntoView(list.items().getAt(static_cast<uint32_t>(saved)), ScrollIntoViewAlignment::Leading);
    }
};
auto const model = gallery::hold<Model>();

auto const* people = &gallery::contacts();
model->list.itemTemplate([people](Object const& item) {
    auto const position = static_cast<size_t>(intOf(item));
    std::u16string text = core::to_u16(static_cast<int>(position) + 1).plain();
    text += u". ";
    text += (*people)[position].name();
    return TextBlock {Margin {0, 5, 0, 5}, text};
});
model->list.itemsSource(indexList(static_cast<int64_t>(people->size())));
model->restore.add_onClick([model](auto&&...) { model->bring(); });

auto example = StackPanel {
    model->list,
    StackPanel {
        orientation.horizontal,
        spacing = 8,
        Margin {0, 8, 0, 0},
        Button {content = u"Save position", onClick = [model](Button const&) { model->save(); }},
        model->restore,
    },
};