auto example = StackPanel {
    spacing = 12.0,
    Border {cornerRadius = CornerRadius {4},
            child = Image {height = 150, hAlign.left, automationName = u"Grapes", source = u"Assets/SampleMedia/grapes.jpg"}},
    RichTextBlock {fontSize = 12,
                   Paragraph {Run {u"The image above will be read out as "}, Bold {Run {u"Grapes"}},
                              Run {u". To navigate in Narrator through elements that aren't focusable, use Caps + Left/Right arrow."}}},
};
