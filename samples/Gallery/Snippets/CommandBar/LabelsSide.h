// The bar is open while the field says so, and sticky with it, as the
// buttons of the original do: open and sticky, closed and not. The user opens
// and closes it too, with the ellipsis, and the bar says so back.
struct Model {
    core::observable<bool> open{false};
};
auto const model = gallery::hold<Model>();
auto selected = TextBlock {Padding {0, 8, 0, 0}};

auto command = [selected](Symbol which, char16_t const* caption, VirtualKey pressed, VirtualKeyModifiers held) {
    return AppBarButton {
        icon = SymbolIcon {symbol = which},
        label = caption,
        keyboardAccelerators[KeyboardAccelerator {key = pressed, modifiers = held}],
        onClick = [selected, caption](AppBarButton const&) { selected.text(std::u16string {u"You clicked: "} + caption); },
    };
};

auto bar = CommandBar {
    background = colors.transparent,
    defaultLabelPosition = CommandBarDefaultLabelPosition::Right,
    isOpen = BindOutput {model->open},
    isSticky = BindOutput {model->open},
    onOpened = [model] { model->open.set(true); },
    onClosed = [model] { model->open.set(false); },
    primaryCommands[
        command(Symbol::Add, u"Add", VirtualKey::A, VirtualKeyModifiers::Control),
        command(Symbol::Edit, u"Edit", VirtualKey::E, VirtualKeyModifiers::Control),
        command(Symbol::Share, u"Share", VirtualKey::F4, VirtualKeyModifiers::None)
    ],
    secondaryCommands[command(Symbol::Setting, u"Settings", VirtualKey::I, VirtualKeyModifiers::Control)],
};

// The four buttons the original adds to the secondary commands, and takes away.
auto addSecondary = [bar, selected]() {
    auto const secondary = bar.secondaryCommands();
    if (secondary.size() != 1) {
        return;
    }
    auto append = [&](Symbol which, char16_t const* caption, VirtualKey pressed, VirtualKeyModifiers held) {
        secondary.append(AppBarButton {
            icon = SymbolIcon {symbol = which},
            label = caption,
            keyboardAccelerators[KeyboardAccelerator {key = pressed, modifiers = held}],
            onClick = [selected, caption](AppBarButton const&) { selected.text(std::u16string {u"You clicked: "} + caption); },
        });
    };
    append(Symbol::Add, u"Button 1", VirtualKey::N, VirtualKeyModifiers::Control);
    append(Symbol::Delete, u"Button 2", VirtualKey::Delete, VirtualKeyModifiers::None);
    secondary.append(AppBarSeparator {});
    append(Symbol::FontDecrease, u"Button 3", VirtualKey::Subtract, VirtualKeyModifiers::Control);
    append(Symbol::FontIncrease, u"Button 4", VirtualKey::Add, VirtualKeyModifiers::Control);
};

auto removeSecondary = [bar]() {
    auto const secondary = bar.secondaryCommands();
    while (secondary.size() > 1) {
        secondary.removeAtEnd();
    }
};

auto example = StackPanel {bar, selected};

auto options = StackPanel {
    TextBlock {u"Show or hide"},
    Button {Margin {0, 12, 0, 0}, content = u"Open command bar", onClick = [model] { model->open.set(true); }},
    Button {Margin {0, 12, 0, 0}, content = u"Close command bar", onClick = [model] { model->open.set(false); }},
    TextBlock {Margin {0, 16, 0, 0}, u"Modify content"},
    Button {Margin {0, 12, 0, 0}, content = u"Add secondary commands", onClick = [addSecondary] { addSecondary(); }},
    Button {Margin {0, 12, 0, 0}, content = u"Remove secondary commands", onClick = [removeSecondary] { removeSecondary(); }},
};