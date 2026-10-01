struct Model {
    core::observable<double> x1{0};
    core::observable<double> y1{0};
    core::observable<double> x2{200};
    core::observable<double> y2{0};
    core::observable<double> thickness{5};
};
auto const model = gallery::hold<Model>();

auto example = Canvas {
    width = 100,
    height = 200,
    Line {
        top = 50,
        stroke = rgb(0x46, 0x82, 0xb4),
        strokeThickness = BindOutput {model->thickness},
        x1 = BindOutput {model->x1},
        dsl::y1 = BindOutput {model->y1},
        x2 = BindOutput {model->x2},
        y2 = BindOutput {model->y2},
    },
};

auto options = StackPanel {
    width = 220,
    Slider {header = u"Start point X", isFocusEngagementEnabled = false, minimum = 0.0, maximum = 100.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->x1}},
    Slider {header = u"Start point Y", isFocusEngagementEnabled = false, minimum = 0.0, maximum = 100.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->y1}},
    Slider {header = u"End point X", isFocusEngagementEnabled = false, minimum = 200.0, maximum = 300.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->x2}},
    Slider {header = u"End point Y", isFocusEngagementEnabled = false, minimum = 0.0, maximum = 100.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->y2}},
    Slider {header = u"Stroke Thickness", isFocusEngagementEnabled = false, minimum = 5.0, maximum = 10.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->thickness}},
};