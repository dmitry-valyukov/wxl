auto output = TextBlock {};

auto item = [output](char16_t const* caption, VirtualKey pressed) {
    return MenuFlyoutItem {
        text = caption,
        keyboardAccelerators[KeyboardAccelerator {key = pressed, modifiers = VirtualKeyModifiers::Control}],
        onClick = [output, caption](MenuFlyoutItem const&) { output.text(std::u16string {u"You clicked: "} + caption); },
    };
};

auto bar = MenuBar {
    MenuBarItem {
        title = u"File",
        item(u"New", VirtualKey::N),
        item(u"Open", VirtualKey::O),
        item(u"Save", VirtualKey::S),
        item(u"Exit", VirtualKey::E),
    },
    MenuBarItem {
        title = u"Edit",
        item(u"Undo", VirtualKey::Z),
        item(u"Cut", VirtualKey::X),
        item(u"Copy", VirtualKey::C),
        item(u"Paste", VirtualKey::V),
    },
    MenuBarItem {title = u"Help", item(u"About", VirtualKey::I)},
};