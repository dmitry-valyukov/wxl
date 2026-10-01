struct Model {
    core::observable<double> height{100};
    core::observable<double> width{100};
    core::observable<double> thickness{2};
    core::observable<double> radiusX{0};
    core::observable<double> radiusY{0};
};
auto const model = gallery::hold<Model>();

auto example = Rectangle {
    width = BindOutput {model->width},
    height = BindOutput {model->height},
    fill = rgb(0x46, 0x82, 0xb4),
    radiusX = BindOutput {model->radiusX},
    radiusY = BindOutput {model->radiusY},
    stroke = colors.black,
    strokeThickness = BindOutput {model->thickness},
};

auto options = StackPanel {
    width = 220,
    Slider {header = u"Height", isFocusEngagementEnabled = false, minimum = 100.0, maximum = 150.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->height}},
    Slider {header = u"Width", isFocusEngagementEnabled = false, minimum = 100.0, maximum = 150.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->width}},
    Slider {header = u"Stroke Thickness", isFocusEngagementEnabled = false, minimum = 2.0, maximum = 10.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->thickness}},
    Slider {header = u"Radius Y", isFocusEngagementEnabled = false, minimum = 0.0, maximum = 100.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->radiusY}},
    Slider {header = u"Radius X", isFocusEngagementEnabled = false, minimum = 0.0, maximum = 100.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->radiusX}},
};