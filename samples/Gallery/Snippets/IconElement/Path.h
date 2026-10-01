auto example = StackPanel {
    TextBlock {
        Margin {0, 0, 0, 12},
        u"To use a PathIcon as the icon for a control, you specify the path data of the image you are trying to display. "
        u"The path data draws a series of connected lines and curves.",
        styles.TextBlock.Body,
    },
    Button {content = PathIcon {hAlign.center, data = u"F1 M 16,12 20,2L 20,16 1,16"}},
};