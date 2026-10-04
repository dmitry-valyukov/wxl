auto output = TextBlock {automationId = u"Control2Output"};

auto box = CheckBox {
    automationName = u"Three-state",
    content = u"Three-state CheckBox",
    isThreeState = true,
    onChecked = [output](Object const&, RoutedEventArgs&) { output.text(u"CheckBox is checked."); },
    onUnchecked = [output](Object const&, RoutedEventArgs&) { output.text(u"CheckBox is unchecked."); },
    onIndeterminate = [output](Object const&, RoutedEventArgs&) {
        output.text(u"CheckBox state is indeterminate.");
    },
};