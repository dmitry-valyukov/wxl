auto example = StackPanel {
    TextBlock {
        Margin {0, 0, 0, 12},
        u"To use a SymbolIcon as the icon for a control, you specify the enum value for the glyph you would like to "
        u"display. SymbolIcon's enum is based off of icons from the Segoe MDL2 font used by Windows 10.",
        styles.TextBlock.Body,
    },
    Button {content = StackPanel {SymbolIcon {symbol = Symbol::Accept}, TextBlock {u"Accept"}}},
};