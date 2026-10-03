auto example = StackPanel {
    spacing = 12.0,
    Button {content = u"Download survey"},
    RichTextBlock {fontSize = 12,
                   Paragraph {Run {u"Screen readers will read this button as "}, Bold {Run {u"Download survey"}},
                              Run {u". The name is automatically derived from the Button's Content."}}},
};
