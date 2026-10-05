// The data the tree shows: a folder has children, a file has none.
struct ExplorerItem {
    enum class Kind { Folder, File };
    std::u16string name;
    Kind kind;
    std::vector<ExplorerItem> children;
};
std::vector<ExplorerItem> const data = {
    {u"Documents", ExplorerItem::Kind::Folder,
     {{u"ProjectProposal", ExplorerItem::Kind::File, {}}, {u"BudgetReport", ExplorerItem::Kind::File, {}}}},
    {u"Projects", ExplorerItem::Kind::Folder, {{u"Project Plan", ExplorerItem::Kind::File, {}}}},
};
// The template is chosen by the kind of the item, as the DataTemplateSelector of the original chooses it:
// a folder is a picture and a name, a file is a glyph and a name.
auto const contentOf = [](ExplorerItem const& item) -> StackPanel {
    if (item.kind == ExplorerItem::Kind::Folder) {
        return StackPanel {orientation.horizontal, spacing = 10,
                           Image {width = 20, source = u"Assets/SampleMedia/folder.png"}, TextBlock {item.name}};
    }
    return StackPanel {orientation.horizontal, spacing = 10, FontIcon {glyph = u"\uE8A5"}, TextBlock {item.name}};
};

auto const nodeOf = [contentOf](auto const& self, ExplorerItem const& item) -> TreeViewNode {
    auto node = TreeViewNode {content = contentOf(item), isExpanded = true};
    for (auto const& child : item.children) {
        node.children().append(self(self, child));
    }
    return node;
};

// The nodes hold elements; a template that shows an item as the element it is is XAML text only, and the tag has it.
auto tree = TreeView {
    elementItems = true,
    minWidth = 345,
    maxHeight = 400,
    Margin {0, 12, 0, 0},
    horizontalAlignment = HorizontalAlignment::Center,
    verticalAlignment = VerticalAlignment::Top,
};
for (auto const& item : data) {
    tree.rootNodes().append(nodeOf(nodeOf, item));
}

auto example = Border {height = 200, borderBrush = brushes.Card.StrokeColorDefault, borderThickness = 1, tree};