// The tree of the original, built by hand: nodes with their text, folders opened.
auto const workFolder = TreeViewNode {u"Work Documents", isExpanded = true};
workFolder.children().append(TreeViewNode {u"XYZ Functional Spec"});
workFolder.children().append(TreeViewNode {u"Feature Schedule"});

auto const remodelFolder = TreeViewNode {u"Home Remodel", isExpanded = true};
remodelFolder.children().append(TreeViewNode {u"Contractor Contact Info"});
remodelFolder.children().append(TreeViewNode {u"Paint Color Scheme"});

auto const personalFolder = TreeViewNode {u"Personal Documents", isExpanded = true};
personalFolder.children().append(remodelFolder);
auto tree = TreeView {
    minWidth = 345,
    maxHeight = 400,
    Margin {0, 12, 0, 0},
    horizontalAlignment = HorizontalAlignment::Center,
    verticalAlignment = VerticalAlignment::Top,
    selectionMode = TreeViewSelectionMode::Multiple,
};
tree.rootNodes().append(workFolder);
tree.rootNodes().append(personalFolder);

auto example = Border {height = 280, borderBrush = brushes.Card.StrokeColorDefault, borderThickness = 1, tree};