auto viewbox = Viewbox {
    width = 200,
    height = 200,
    vAlign.top,
    stretchDirection = StretchDirection::Both,
    Border {
        borderBrush = colors.gray,
        BorderThickness {15},
        StackPanel {
            background = rgb(169, 169, 169),
            StackPanel {
                orientation.horizontal,
                Rectangle {width = 40, height = 10, fill = colors.blue},
                Rectangle {width = 40, height = 10, fill = colors.green},
                Rectangle {width = 40, height = 10, fill = colors.red},
                Rectangle {width = 40, height = 10, fill = rgb(255, 255, 0)},
            },
            Image {source = u"Assets/Slices.png"},
            TextBlock {u"This is text.", hAlign.center},
        },
    },
};

auto size = Slider {
    header = u"Width/Height",
    minimum = 20.0,
    maximum = 300.0,
    value = 200.0,
    onValueChanged = [viewbox](Slider const& self) {
        viewbox.width(self.value());
        viewbox.height(self.value());
    },
};

auto stretchGroup = RadioButtons {
    header = u"Stretch",
    RadioButton {content = u"None"},
    RadioButton {content = u"Fill"},
    RadioButton {content = u"Uniform"},
    RadioButton {content = u"UniformToFill"},
    selectedIndex = 2,
    onSelectionChanged = [viewbox](RadioButtons const& self) {
        static constexpr Stretch modes[] = {Stretch::None, Stretch::Fill, Stretch::Uniform, Stretch::UniformToFill};
        int const index = self.selectedIndex();
        if (index >= 0) {
            viewbox.stretch(modes[index]);
        }
    },
};

auto directionGroup = RadioButtons {
    header = u"StretchDirection",
    RadioButton {content = u"UpOnly"},
    RadioButton {content = u"DownOnly"},
    RadioButton {content = u"Both"},
    selectedIndex = 2,
    onSelectionChanged = [viewbox](RadioButtons const& self) {
        static constexpr StretchDirection modes[] = {StretchDirection::UpOnly, StretchDirection::DownOnly,
                                                     StretchDirection::Both};
        int const index = self.selectedIndex();
        if (index >= 0) {
            viewbox.stretchDirection(modes[index]);
        }
    },
};