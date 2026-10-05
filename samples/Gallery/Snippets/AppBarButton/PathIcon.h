auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarButton {
    content = Viewbox {stretch = Stretch::Uniform, child = PathIcon {data = u"F1 M 20,20L 24,10L 24,24L 5,24"}},
    label = u"PathIcon",
    onClick = [output](AppBarButton const&) { output.text(u"You clicked: Button4"); },
};