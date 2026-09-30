auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarButton {
    icon = SymbolIcon {symbol = Symbol::Like},
    label = u"SymbolIcon",
    onClick = [output](AppBarButton const&) { output.text(u"You clicked: Button1"); },
};