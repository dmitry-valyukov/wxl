// The sliders and the choices write fields of a model, and the border shows them: the way of x:Bind with a model.
struct Model {
    core::observable<double> thickness {2.0};
    core::observable<int> background {3}, brush {1};
};
auto const model = gallery::hold<Model>();

auto border = Border {
    vAlign.top,
    background = BindOutput {model->background, [](int index) {
        static constexpr Color paints[] = {rgb(0, 128, 0), rgb(255, 255, 0), rgb(0, 0, 255), rgb(255, 255, 255)};
        return SolidColorBrush {color = paints[std::max(index, 0)]};
    }},
    borderBrush = BindOutput {model->brush, [](int index) {
        static constexpr Color paints[] = {rgb(0, 100, 0), rgb(255, 215, 0), rgb(0, 0, 139), rgb(255, 255, 255)};
        return SolidColorBrush {color = paints[std::max(index, 0)]};
    }},
    borderThickness = BindOutput {model->thickness, [](double width) { return Thickness {width}; }},
    TextBlock {u"Text inside a border", Margin {8, 5}, fontSize = 18, foreground = colors.black},
};

auto thickness = Slider {
    header = u"BorderThickness",
    minimum = 0.0,
    maximum = 10.0,
    stepFrequency = 1.0,
    value = Bind {model->thickness},
};

auto backgroundChoice = RadioButtons {
    header = u"Background",
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"Blue"},
    RadioButton {content = u"White"},
    selectedIndex = Bind {model->background},
};

auto brushChoice = RadioButtons {
    header = u"BorderBrush",
    column = 1,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"Blue"},
    RadioButton {content = u"White"},
    selectedIndex = Bind {model->brush},
};
