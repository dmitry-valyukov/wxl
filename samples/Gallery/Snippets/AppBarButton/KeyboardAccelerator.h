auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarButton {
    icon = SymbolIcon {symbol = Symbol::Save},
    label = u"Save",
    keyboardAccelerators[KeyboardAccelerator {key = VirtualKey::S, modifiers = VirtualKeyModifiers::Control}],
    onClick = [output](AppBarButton const&) { output.text(u"You clicked: Button5"); },
};