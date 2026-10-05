struct Model {
    core::observable<double> size {200.0};
    core::observable<int> stretch {2}, direction {2};
};
auto const model = gallery::hold<Model>();

auto viewbox = Viewbox {
    width = BindOutput {model->size},
    height = BindOutput {model->size},
    vAlign.top,
    stretch = BindOutput {model->stretch, [](int index) { return static_cast<Stretch>(index); }},
    stretchDirection = BindOutput {model->direction, [](int index) { return static_cast<StretchDirection>(index); }},
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
    value = Bind {model->size},
};

auto stretchGroup = RadioButtons {
    header = u"Stretch",
    RadioButton {content = u"None"},
    RadioButton {content = u"Fill"},
    RadioButton {content = u"Uniform"},
    RadioButton {content = u"UniformToFill"},
    selectedIndex = Bind {model->stretch},
};

auto directionGroup = RadioButtons {
    header = u"StretchDirection",
    RadioButton {content = u"UpOnly"},
    RadioButton {content = u"DownOnly"},
    RadioButton {content = u"Both"},
    selectedIndex = Bind {model->direction},
};
