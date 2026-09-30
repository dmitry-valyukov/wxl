auto link = HyperlinkButton {
    content = u"Microsoft home page",
    navigateUri = u"https://www.microsoft.com",
};

auto disable = CheckBox {
    content = u"Disable hyperlink button",
    onChecked = [link](Object const&, RoutedEventArgs&) { link.isEnabled(false); },
    onUnchecked = [link](Object const&, RoutedEventArgs&) { link.isEnabled(true); },
};