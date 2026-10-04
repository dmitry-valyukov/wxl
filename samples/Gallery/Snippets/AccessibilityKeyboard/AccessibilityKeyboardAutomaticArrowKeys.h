auto example = StackPanel {
    orientation.vertical,
    // ListViews fully support arrow keys, for example
    ListView {
        width = 300,
        automationName = u"Colors",
        items[ListViewItem {content = u"Red"}, ListViewItem {content = u"Blue"}, ListViewItem {content = u"Green"},
              ListViewItem {content = u"Yellow"}],
    },
    TextBlock {styles.TextBlock.Caption, u"Tab navigates to the control, arrow keys navigate within the control"},
};
