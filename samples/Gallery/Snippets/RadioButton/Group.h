auto output = TextBlock {u"Select an option."};

auto group = RadioButtons {
    header = u"Options:",
    RadioButton {
        automationId = u"Option1RadioButton",
        content = u"Option 1",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 1"); },
    },
    RadioButton {
        automationId = u"Option2RadioButton",
        content = u"Option 2",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 2"); },
    },
    RadioButton {
        automationId = u"Option3RadioButton",
        content = u"Option 3",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 3"); },
    },
};