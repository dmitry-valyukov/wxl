auto button = AppBarButton {
    allowFocusOnInteraction = true,
    icon = SymbolIcon {symbol = Symbol::Edit},
    label = u"Edit",
    flyout = Flyout {TextBox {minWidth = 240, placeholderText = u"Input text here"}},
};