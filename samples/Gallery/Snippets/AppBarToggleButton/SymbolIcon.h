auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarToggleButton {
    icon = SymbolIcon {symbol = Symbol::Shuffle},
    label = u"SymbolIcon",
    onClick = [output](AppBarToggleButton const& self) { output.text(self.isChecked() ? u"IsChecked = True" : u"IsChecked = False"); },
};