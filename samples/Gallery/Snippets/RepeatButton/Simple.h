auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = RepeatButton {
    content = u"Click and hold",
    onClick = [output, clicks = std::make_shared<int>(0)] {
        output.text(L"Number of clicks: " + std::to_wstring(++*clicks));
    },
};

auto disable = CheckBox {
    content = u"Disable RepeatButton",
    onChecked = [button](Object const&, RoutedEventArgs&) { button.isEnabled(false); },
    onUnchecked = [button](Object const&, RoutedEventArgs&) { button.isEnabled(true); },
};