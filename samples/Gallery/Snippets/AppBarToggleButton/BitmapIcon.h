auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarToggleButton {
    icon = BitmapIcon {uriSource = u"Assets/SampleMedia/Slices2.png"},
    label = u"BitmapIcon",
    onClick = [output](AppBarToggleButton const& self) { output.text(self.isChecked() ? u"IsChecked = True" : u"IsChecked = False"); },
};