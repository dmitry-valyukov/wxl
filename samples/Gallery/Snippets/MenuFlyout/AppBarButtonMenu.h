auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto item = [output](char16_t const* caption, char16_t const* option) {
    return MenuFlyoutItem {
        text = caption,
        onClick = [output, option](MenuFlyoutItem const&) { output.text(std::u16string {u"Sort by: "} + option); },
    };
};

auto button = AppBarButton {
    icon = SymbolIcon {symbol = Symbol::Sort},
    isCompact = true,
    toolTip = u"Sort",
    flyout = MenuFlyout {item(u"By rating", u"rating"), item(u"By match", u"match"), item(u"By distance", u"distance")},
};