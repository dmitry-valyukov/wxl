auto output = TextBlock {automationId = u"Control1Output"};

auto box = CheckBox {
    automationName = u"Two-state",
    content = u"Two-state CheckBox",
    onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You checked the box."); },
    onUnchecked = [output](Object const&, RoutedEventArgs&) { output.text(u"You unchecked the box."); },
};