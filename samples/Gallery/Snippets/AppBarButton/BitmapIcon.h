auto output = TextBlock {Margin {8, 0, 0, 0}, vAlign.center};

auto button = AppBarButton {
    icon = BitmapIcon {uriSource = u"Assets/SampleMedia/Slices2.png"},
    label = u"BitmapIcon",
    onClick = [output](AppBarButton const&) { output.text(u"You clicked: Button2"); },
};