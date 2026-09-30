auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarButton {
    icon = FontIcon {fontFamily = u"Candara", glyph = u"\u03A3"},
    label = u"FontIcon",
    onClick = [output](AppBarButton const&) { output.text(u"You clicked: Button3"); },
};