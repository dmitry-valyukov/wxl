struct Model {
    core::observable<int> direction {1};
    core::observable<double> gap {8.0};
};
auto const model = gallery::hold<Model>();

const Preset square {width = 40, height = 40};

auto panel = StackPanel {
    vAlign.top,
    spacing = BindOutput {model->gap},
    orientation = BindOutput {model->direction, [](int index) { return index == 0 ? Orientation::Horizontal : Orientation::Vertical; }},
    Rectangle {square, fill = colors.red},
    Rectangle {square, fill = colors.blue},
    Rectangle {square, fill = colors.green},
    Rectangle {square, fill = rgb(255, 255, 0)},
};

auto orientationGroup = RadioButtons {
    header = u"Orientation",
    RadioButton {content = u"Horizontal"},
    RadioButton {content = u"Vertical"},
    selectedIndex = Bind {model->direction},
};

auto gapSlider = Slider {
    header = u"Spacing",
    minimum = 0.0,
    maximum = 16.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    value = Bind {model->gap},
};
