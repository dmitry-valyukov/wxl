auto example = StackPanel {
    TextBlock {
        Margin {0, 0, 0, 12},
        u"To use an ImageIcon as the icon for a control, you can set image that has a file format supported by the "
        u"Image class. The two examples here show a PNG and SVG image as the icon.",
        styles.TextBlock.Body,
    },
    Button {width = 100, automationName = u"ImageExample1", content = ImageIcon {source = u"Assets/SampleMedia/Slices.png"}},
};