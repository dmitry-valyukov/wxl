auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarToggleButton {
    content = Viewbox {child = PathIcon {data = u"F1 M 20,20L 24,10L 24,24L 5,24"}},
    label = u"PathIcon",
    isThreeState = true,
    onClick = [output](AppBarToggleButton const& self) {
        auto const state = self.isChecked();
        output.text(!state ? u"IsChecked = null" : *state ? u"IsChecked = True" : u"IsChecked = False");
    },
};