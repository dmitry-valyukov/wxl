// Many controls automatically indicate position in set
auto const students = TextBlock {automationAccessibilityView = AccessibilityView::Raw, styles.TextBlock.BodyStrong, u"Students"};
auto example = StackPanel {
    students,
    ListView {
        automationLabeledBy = students,
        items[ListViewItem {content = u"Nathan Quinn"}, ListViewItem {content = u"Jessica Lamber"}, ListViewItem {content = u"Carl Bond"},
              ListViewItem {content = u"Jessica Russel"}],
    },
    // Custom layouts may need to specify PositionInSet and SizeOfSet manually.
    StackPanel {
        orientation.horizontal,
        spacing = 8.0,
        Button {automationPositionInSet = 1, automationSizeOfSet = 3, content = u"View"},
        Button {automationPositionInSet = 2, automationSizeOfSet = 3, content = u"Rename"},
        Button {automationPositionInSet = 3, automationSizeOfSet = 3, content = u"Delete"},
    },
};
