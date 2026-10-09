// The model keeps an observable_list, and the list control is bound to it together with the function that builds the
// element of an item: `itemsSource = BindOutput {list, build}`. Each change of the list reaches the control as that
// change -- a name added is one row more, the rows already shown stay as they are -- and the count is a field of the
// list. A click hands out the item as the control holds it; boundItem gives back the item of the model.
struct Model {
    core::observable_list<core::u16_text> names;
    core::observable<core::u16_text> clicked;
    int added = 0;

    void add() { names.push_back(core::format(u"Item {}", ++added)); }
    void addFirst() { names.insert(0, core::format(u"Item {}", ++added)); }
    void removeFirst() {
        if (!names.empty()) names.erase(0);
    }
};
auto const model = gallery::hold<Model>();
model->add();
model->add();
auto* const shelf = model.get();

auto example = StackPanel {
    spacing = 8.0,
    StackPanel {orientation.horizontal, spacing = 8.0,
                Button {content = u"Add", onClick = [shelf](auto&&...) { shelf->add(); }},
                Button {content = u"Add first", onClick = [shelf](auto&&...) { shelf->addFirst(); }},
                Button {content = u"Remove first", onClick = [shelf](auto&&...) { shelf->removeFirst(); }}},
    TextBlock {text = BindOutput {model->names.count(), [](uint32_t count) { return core::format(u"{} items", count); }}},
    ListView {
        height = 200,
        borderBrush = brushes.Card.StrokeColorDefault,
        BorderThickness {1},
        cornerRadius = CornerRadius {4},
        selectionMode = ListViewSelectionMode::None,
        isItemClickEnabled = true,
        itemsSource = BindOutput {model->names, [](core::u16_text const& name) { return TextBlock {text = name}; }},
        onItemClick = [shelf](ListView const&, ItemClickEventArgs& args) {
            if (core::u16_text const* name = boundItem(shelf->names, args.clickedItem())) {
                shelf->clicked.set(*name);
            }
        },
    },
    TextBlock {text = BindOutput {model->clicked, [](core::u16_text const& name) { return core::format(u"Clicked: {}", name); }}},
};
