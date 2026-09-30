auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center, fontFamily = u"Global User Interface"};

// What the command says of itself -- label, icon, key, description -- is the
// command's, and every control that takes it shows it.
auto custom = XamlUICommand {
    description = u"This is a custom command",
    label = u"Custom Command",
    iconSource = SymbolIconSource {symbol = Symbol::Favorite},
    keyboardAccelerators[KeyboardAccelerator {key = VirtualKey::D, modifiers = VirtualKeyModifiers::Control}],
    onExecuteRequested = [output](XamlUICommand const&, ExecuteRequestedEventArgs&) { output.text(u"You fired the custom command"); },
};

auto example = Grid {
    rowDefinitions = u"auto,auto,*",
    TextBlock {
        Margin {0, 0, 0, 12},
        textWrapping = TextWrapping::Wrap,
        u"XamlUICommand allows the sharing of the UX associated with a command. "
        u"In this instance we create a simple Custom Command with a label, icon, shortcut, and description. "
        u"It's defined as a resource and could be used in many controls, like this AppBarButton. "
        u"The button (and other controls) automatically gets all these UI properties, without the need to define the properties again.",
    },
    StackPanel {row = 1, orientation.horizontal, AppBarButton {command = custom}, output},
};