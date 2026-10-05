auto example = StackPanel {
    spacing = 8.0,
    TextBlock {styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap,
               u"The sample below groups items using accessible names. Screen readers will read this as users navigate between different "
               u"groups. To force read the current context in Narrator, press Caps+/."},
    TextBlock {styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap,
               u"Note how Narrator reads 'My albums' or 'Shared with me' if you tab between the ListViews, and reads the additional context "
               u"'Album browser' when Caps+/ is pressed."},
    StackPanel {
        automationName = u"Album browser",
        StackPanel {
            automationName = u"My albums",
            // These TextBlocks could reasonably be headings, too.
            TextBlock {styles.TextBlock.BodyStrong, u"My albums"},
            ListView {items[ListViewItem {content = u"Trip to Redmond"}, ListViewItem {content = u"Visiting Ben"}]},
        },
        StackPanel {
            automationName = u"Shared with me",
            TextBlock {styles.TextBlock.BodyStrong, u"Shared with me"},
            ListView {items[ListViewItem {content = u"Valeria's cat"}, ListViewItem {content = u"Paul's winter vacation"},
                            ListViewItem {content = u"Cool street photography"}]},
        },
    },
};
