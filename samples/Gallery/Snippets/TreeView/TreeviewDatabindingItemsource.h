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
// What the DataTemplate of the original says -- the item's name as the content of a node, a folder opened,
// its children as the nodes below -- is a function from an item to a node.
auto const nodeOf = [](auto const& self, ExplorerItem const& item) -> TreeViewNode {
    auto node = TreeViewNode {content = item.name, isExpanded = true};
    for (auto const& child : item.children) {
        node.children().append(self(self, child));
    }
    return node;
};

auto tree = TreeView {
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