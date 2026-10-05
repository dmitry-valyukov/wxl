struct Model {
    core::observable<int> background {0}, border {1};
};
auto const model = gallery::hold<Model>();

auto sample = Border {
    height = 50,
    Margin {0, 10, 0, 10},
    background = BindOutput {model->background, [](int index) {
        static constexpr Color colors[] = {rgb(0, 128, 0), rgb(255, 255, 0), rgb(255, 255, 255)};
        return SolidColorBrush {color = colors[std::max(index, 0)]};
    }},
    borderBrush = BindOutput {model->border, [](int index) {
        static constexpr Color colors[] = {rgb(0, 100, 0), rgb(255, 215, 0), rgb(255, 255, 255)};
        return SolidColorBrush {color = colors[std::max(index, 0)]};
    }},
    BorderThickness {10},
};

auto background = RadioButtons {
    header = u"Background",
    maxColumns = 3,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"White"},
    selectedIndex = Bind {model->background},
};

auto border = RadioButtons {
    header = u"Border",
    maxColumns = 3,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"White"},
    selectedIndex = Bind {model->border},
};
