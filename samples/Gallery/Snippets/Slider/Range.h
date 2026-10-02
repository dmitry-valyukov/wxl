auto output = TextBlock {u"800"};

auto slider = Slider {
    width = 200,
    Margin {0, 0, 10, 0},
    header = u"Control header",
    minimum = 500.0,
    maximum = 1000.0,
    smallChange = 10.0,
    stepFrequency = 10.0,
    value = 800.0,
    onValueChanged = [output](Slider const& self) {
        output.text(core::to_u16(self.value(), std::chars_format::fixed, 0));
    },
};

const Preset field {
    spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Compact,
    Margin {10, 5, 0, 0},
    minWidth = 80,
};

auto options = Grid {
    rowDefinitions = u"*,*,*,*",
    columnDefinitions = u"auto,auto",
    TextBlock {u"Minimum:", row = 0, column = 0},
    NumberBox {
        field, row = 0, column = 1, value = 500.0,
        onValueChanged = [slider](NumberBox const& self) { slider.minimum(self.value()); },
    },
    TextBlock {u"Maximum:", row = 1, column = 0, Margin {0, 7, 0, 0}},
    NumberBox {
        field, row = 1, column = 1, value = 1000.0,
        onValueChanged = [slider](NumberBox const& self) { slider.maximum(self.value()); },
    },
    TextBlock {u"StepFrequency:", row = 2, column = 0, Margin {0, 5, 0, 0}},
    NumberBox {
        field, row = 2, column = 1, minimum = 1.0, value = 10.0,
        onValueChanged = [slider](NumberBox const& self) { slider.stepFrequency(self.value()); },
    },
    TextBlock {u"SmallChange:", row = 3, column = 0, Margin {0, 5, 0, 0}},
    NumberBox {
        field, row = 3, column = 1, value = 10.0,
        onValueChanged = [slider](NumberBox const& self) { slider.smallChange(self.value()); },
    },
};