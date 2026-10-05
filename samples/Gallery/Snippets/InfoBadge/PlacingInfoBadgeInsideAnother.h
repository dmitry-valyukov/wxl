Button {
    width = 200,
    height = 60,
    Padding {0},
    hAlign.center,
    horizontalContentAlignment = HorizontalAlignment::Stretch,
    verticalContentAlignment = VerticalAlignment::Stretch,
    toolTip = u"Refresh required",
    content = Grid {
        hAlign.stretch,
        vAlign.stretch,
        SymbolIcon {hAlign.center, symbol = FluentSymbol::Sync},
        InfoBadge {
            hAlign.right,
            vAlign.top,
            background = rgb(196, 43, 28),
            iconSource = FontIconSource {fontFamily = u"Segoe Fluent Icons", glyph = u"\uF13C"},
        },
    },
}