auto output = TextBlock {u"Off"};

auto toggle = ToggleButton {
    content = u"ToggleButton",
    onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"On"); },
    onUnchecked = [output](Object const&, RoutedEventArgs&) { output.text(u"Off"); },
};

auto disable = CheckBox {
    content = u"Disable ToggleButton",
    onChecked = [toggle](Object const&, RoutedEventArgs&) { toggle.isEnabled(false); },
    onUnchecked = [toggle](Object const&, RoutedEventArgs&) { toggle.isEnabled(true); },
};