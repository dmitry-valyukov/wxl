struct Model {
    core::observable<double> height{100};
    core::observable<double> width{100};
    core::observable<double> thickness{2};
};
auto const model = gallery::hold<Model>();

auto example = Ellipse {
    width = BindOutput {model->width},
    height = BindOutput {model->height},
    fill = rgb(0x46, 0x82, 0xb4),
    stroke = colors.black,
    strokeThickness = BindOutput {model->thickness},
};

auto options = StackPanel {
    width = 220,
    Slider {header = u"Height", isFocusEngagementEnabled = false, minimum = 100.0, maximum = 150.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->height}},
    Slider {header = u"Width", isFocusEngagementEnabled = false, minimum = 100.0, maximum = 150.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->width}},
    Slider {header = u"Stroke Thickness", isFocusEngagementEnabled = false, minimum = 2.0, maximum = 10.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->thickness}},
};