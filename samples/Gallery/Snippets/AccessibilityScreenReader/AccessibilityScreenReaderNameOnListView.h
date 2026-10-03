auto example = StackPanel {
    spacing = 12.0,
    ListView {
        width = 300,
        hAlign.left,
        automationName = u"Contacts",
        items[ListViewItem {content = u"Nathan Quinn"}, ListViewItem {content = u"Jessica Lamber"}, ListViewItem {content = u"Carl Bond"},
              ListViewItem {content = u"Jessica Russel"}],
    },
    RichTextBlock {fontSize = 12,
                   Paragraph {Run {u"Screen readers will read this ListView as "}, Bold {Run {u"Contacts"}},
                              Run {u", while each item will be read using its respective text content."}}},
};
