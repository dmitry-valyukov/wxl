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
    ListView left {height = 400, minWidth = 350, Margin {12}, allowDrop = true, canDragItems = true, canReorderItems = true,
                   borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1,
                   selectionMode = ListViewSelectionMode::Single};
    ListView right {height = 400, minWidth = 350, allowDrop = true, canDragItems = true, canReorderItems = true,
                    borderBrush = brushes.Control.Strong.StrokeColorDefault, borderThickness = 1,
                    selectionMode = ListViewSelectionMode::Single};
    std::vector<int64_t> dragged;  // the contacts being dragged: what the drag carries, kept here

    static std::optional<uint32_t> find(ListView const& list, int64_t contact) {
        auto const items = list.items();
        for (uint32_t at = 0; at < items.size(); ++at) {
            if (intOf(items.getAt(at)) == contact) return at;
        }
        return std::nullopt;
    }

    // Where a drop falls: after the items whose middle the pointer has passed.
    static uint32_t index_at(ListView const& list, DragEventArgs& e) {
        auto const panel = list.itemsPanelRoot();
        double const y = e.getPosition(panel).y;
        double offset = 0;
        uint32_t index = 0;
        for (uint32_t at = 0; at < panel.children().size(); ++at) {
            auto const row = panel.children().getAt(at).try_as<FrameworkElement>();
            if (!row) continue;
            if (y < offset + row.actualHeight() / 2) break;
            offset += row.actualHeight();
            ++index;
        }
        return index;
    }

    void drop(ListView const& target, ListView const& other, DragEventArgs& e) {
        auto const deferral = e.getDeferral();
        for (auto const contact : dragged) {
            // Within the list the items are reordered by the list itself; from the other list they are moved here.
            if (find(target, contact)) continue;
            auto const index = std::min(index_at(target, e), target.items().size());
            target.items().insertAt(index, intBox(contact));
            if (auto const at = find(other, contact)) other.items().removeAt(*at);
        }
        e.acceptedOperation(DataPackageOperation::Move);
        deferral.complete();
    }
};
auto const model = gallery::hold<Model>();

for (auto const& list : {model->left, model->right}) {
    list.itemTemplate(contactOf);
}
// The first six contacts on the left, the next three on the right.
for (int64_t index = 0; index < 6; ++index) model->left.items().append(intBox(index));
for (int64_t index = 6; index < 9; ++index) model->right.items().append(intBox(index));

auto const start = [model](auto const&, DragItemsStartingEventArgs& args) {
    model->dragged.clear();
    for (uint32_t at = 0; at < args.items().size(); ++at) model->dragged.push_back(intOf(args.items().getAt(at)));
    args.data().requestedOperation(DataPackageOperation::Move);
};
auto const over = [](auto const&, DragEventArgs& e) { e.acceptedOperation(DataPackageOperation::Move); };
model->left.add_onDragItemsStarting(start);
model->right.add_onDragItemsStarting(start);
model->left.add_onDragOver(over);
model->right.add_onDragOver(over);
model->right.add_onDragEnter([](auto const&, DragEventArgs& e) { e.dragUIOverride().isGlyphVisible(false); });
model->left.add_onDrop([model](auto const&, DragEventArgs& e) { model->drop(model->left, model->right, e); });
model->right.add_onDrop([model](auto const&, DragEventArgs& e) { model->drop(model->right, model->left, e); });

auto example = Grid {
    rowDefinitions = u"auto,auto",
    columnDefinitions = u"*,*",
    TextBlock {styles.TextBlock.Body, columnSpan = 2,
               u"In these ListView controls, you can drag and drop within a list to reorder items, or drag and drop between lists to move items."},
    model->left,
    model->right,
};