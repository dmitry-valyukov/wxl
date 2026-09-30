auto output = TextBlock {};

auto box = CheckBox {
    content = u"Two-state CheckBox",
    onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You checked the box."); },
    onUnchecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You unchecked the box."); },
};