auto example = StackPanel {
    spacing = 16.0,
    HeaderedContentControl {header = u"Name", TextBox {placeholderText = u"Your name"}},
    // The header is any element: here a text with its own look.
    HeaderedContentControl {
        header = TextBlock {u"Volume", foreground = brushes.Text.FillColor.Secondary},
        Slider {minimum = 0, maximum = 100, value = 40, width = 240},
    },
    // No header, no room taken for it.
    HeaderedContentControl {Button {u"Content only"}},
};
