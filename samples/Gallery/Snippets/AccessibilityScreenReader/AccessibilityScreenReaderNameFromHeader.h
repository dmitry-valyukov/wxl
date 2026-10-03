auto example = StackPanel {
    spacing = 12.0,
    // Headers are promoted to name
    TextBox {width = 200, hAlign.left, header = u"Name"},
    // As are placeholders
    TextBox {width = 200, hAlign.left, placeholderText = u"Nickname"},
    // If both are provided, headers are name and placeholders are moved to description
    TextBox {minWidth = 200, hAlign.left, header = u"Email", placeholderText = u"test@example.com"},
    RichTextBlock {
        fontSize = 12,
        Paragraph {Run {u"Screen readers will read these TextBoxes as "}, Bold {Run {u"Name"}}, Run {u", "}, Bold {Run {u"Nickname"}},
                   Run {u" and "}, Bold {Run {u"Email"}}, Run {u". The names are automatically derived from their headers or placeholders."},
                   LineBreak {}},
        Paragraph {Run {u"When both Header and PlaceholderText are present, Header is used as name and PlaceholderText is used as description."}},
    },
};
