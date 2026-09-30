auto output = TextBlock {};

auto button = Button {
    content = u"Standard button",
    onClick = [output] { output.text(u"You clicked: Button1"); },
};

auto disable = CheckBox {
    content = u"Disable button",
    onChecked = [button](Object const&, RoutedEventArgs&) { button.isEnabled(false); },
    onUnchecked = [button](Object const&, RoutedEventArgs&) { button.isEnabled(true); },
};
