auto button = Button {
    content = u"Edit Options",
    flyout = MenuFlyout {
        MenuFlyoutItem {text = u"Share", icon = FontIcon {glyph = u"\uE72D"}},
        MenuFlyoutItem {text = u"Copy", icon = SymbolIcon {symbol = Symbol::Copy}},
        MenuFlyoutItem {text = u"Delete", icon = SymbolIcon {symbol = Symbol::Delete}},
        MenuFlyoutSeparator {},
        MenuFlyoutItem {text = u"Rename"},
        MenuFlyoutItem {text = u"Select"},
    },
};