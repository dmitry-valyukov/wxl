struct Model {
    core::observable<bool> showPoints{false};
    core::observable<double> thickness{2};
};
auto const model = gallery::hold<Model>();

auto caption = [model](char16_t const* text, double x, double y) {
    return TextBlock {
        text,
        left = x,
        top = y,
        zIndex = 1,
        visibility = BindOutput {model->showPoints, [](bool on) { return on ? Visibility::Visible : Visibility::Collapsed; }},
    };
};

auto example = Canvas {
    width = 320,
    height = 170,
    StackPanel {
        TextBlock {Margin {0, 0, 0, 10}, u"Draws a series of connected straight lines."},
        Polyline {
            zIndex = 0,
            points = u"10,100 60,40 200,40 250,100",
            stroke = colors.black,
            strokeThickness = BindOutput {model->thickness},
        },
    },
    caption(u"Point #1: (10,100)", 0, 140),
    caption(u"Point #2: (60,40)", 50, 40),
    caption(u"Point #3: (200,40)", 200, 40),
    caption(u"Point #4: (250,100)", 240, 140),
};

auto options = StackPanel {
    width = 220,
    ToggleSwitch {header = u"Show points", isOn = Bind {model->showPoints}},
    Slider {header = u"Stroke Thickness", isFocusEngagementEnabled = false, minimum = 2.0, maximum = 10.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->thickness}},
};