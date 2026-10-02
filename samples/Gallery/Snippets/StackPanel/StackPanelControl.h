const Preset square {width = 40, height = 40};

auto panel = StackPanel {
    vAlign.top,
    spacing = 8.0,
    Rectangle {square, fill = colors.red},
    Rectangle {square, fill = colors.blue},
    Rectangle {square, fill = colors.green},
    Rectangle {square, fill = rgb(255, 255, 0)},
};

auto orientationGroup = RadioButtons {
    header = u"Orientation",
    RadioButton {content = u"Horizontal"},
    RadioButton {content = u"Vertical"},
    selectedIndex = 1,
    onSelectionChanged = [panel](RadioButtons const& self) {
        int const index = self.selectedIndex();
        if (index >= 0) {
            panel.orientation(index == 0 ? orientation.horizontal : orientation.vertical);
        }
    },
};

auto gapSlider = Slider {
    header = u"Spacing",
    minimum = 0.0,
    maximum = 16.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    value = 8.0,
    onValueChanged = [panel](Slider const& self) { panel.spacing(self.value()); },
};