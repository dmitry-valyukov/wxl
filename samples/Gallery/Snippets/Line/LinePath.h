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
    height = 200,
    StackPanel {
        TextBlock {u"Draws a series of connected lines and curves."},
        Path {
            data = u"M 10,100 C 100,25 300,250 400,75 H 200",
            stroke = rgb(0xb8, 0x86, 0x0b),
            strokeThickness = BindOutput {model->thickness},
        },
    },
    caption(u"Point #1: (10,100)", 0, 130),
    caption(u"Point #2: (100,25)", 40, 75),
    caption(u"Point #3: (300,250)", 280, 175),
    caption(u"Point #4: (400,75)", 360, 60),
    caption(u"Point #5: (200,75)", 170, 60),
};

auto options = StackPanel {
    width = 220,
    ToggleSwitch {header = u"Show points", isOn = Bind {model->showPoints}},
    Slider {header = u"Stroke Thickness", isFocusEngagementEnabled = false, minimum = 2.0, maximum = 10.0, smallChange = 1.0, stepFrequency = 0.5, value = Bind {model->thickness}},
};