auto const label = TextBlock {automationAccessibilityView = AccessibilityView::Raw, styles.TextBlock.Body, u"Searching Photos:"};
auto example = StackPanel {
    spacing = 8.0,
    label,
    TextBox {width = 200, hAlign.left, automationLabeledBy = label},
    RichTextBlock {fontSize = 12,
                   Paragraph {Run {u"The TextBox above is labeled by the TextBlock and will be read as "}, Bold {Run {u"Searching Photos"}},
                              Run {u"."}}},
};
