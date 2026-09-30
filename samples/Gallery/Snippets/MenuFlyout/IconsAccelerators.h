auto chord = [](VirtualKey pressed, VirtualKeyModifiers held) {
    return KeyboardAccelerator {key = pressed, modifiers = held};
};

auto button = Button {
    content = u"Edit Options",
    flyout = MenuFlyout {
        MenuFlyoutItem {
            text = u"Share",
            icon = FontIcon {glyph = u"\uE72D"},
            keyboardAccelerators[chord(VirtualKey::S, VirtualKeyModifiers::Control)],
        },
        MenuFlyoutItem {
            text = u"Copy",
            icon = SymbolIcon {symbol = Symbol::Copy},
            keyboardAccelerators[chord(VirtualKey::C, VirtualKeyModifiers::Control)],
        },
        MenuFlyoutItem {
            text = u"Delete",
            icon = SymbolIcon {symbol = Symbol::Delete},
            keyboardAccelerators[chord(VirtualKey::Delete, VirtualKeyModifiers::None)],
        },
        MenuFlyoutSeparator {},
        MenuFlyoutItem {text = u"Rename"},
        MenuFlyoutItem {text = u"Select"},
    },
};