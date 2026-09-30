auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarToggleButton {
    icon = FontIcon {fontFamily = u"Candara", glyph = u"\u03A3"},
    label = u"FontIcon",
    onClick = [output](AppBarToggleButton const& self) { output.text(self.isChecked() ? u"IsChecked = True" : u"IsChecked = False"); },
};