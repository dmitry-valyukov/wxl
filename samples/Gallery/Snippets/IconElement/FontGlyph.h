auto example = StackPanel {
    TextBlock {
        Margin {0, 0, 0, 12},
        u"Use FontIcon as the icon for a control if you want to specify a Glyph value from a FontFamily. "
        u"Windows 10 uses the Segoe MDL2 Assets FontFamily and that is what this example is showing.",
        styles.TextBlock.Body,
    },
    Button {content = FontIcon {fontFamily = u"Segoe MDL2 Assets", glyph = u"\uE790"}},
};