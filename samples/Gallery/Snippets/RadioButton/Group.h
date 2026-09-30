auto output = TextBlock {u"Select an option."};

auto group = RadioButtons {
    header = u"Options:",
    RadioButton {
        content = u"Option 1",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 1"); },
    },
    RadioButton {
        content = u"Option 2",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 2"); },
    },
    RadioButton {
        content = u"Option 3",
        onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You selected Option 3"); },
    },
};