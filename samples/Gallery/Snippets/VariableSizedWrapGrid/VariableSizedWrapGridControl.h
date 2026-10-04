struct Model {
    core::observable<int> direction {1};
};
auto const model = gallery::hold<Model>();

auto grid = VariableSizedWrapGrid {
    width = 400,
    itemHeight = 44,
    itemWidth = 44,
    maximumRowsOrColumns = 3,
    orientation = BindOutput {model->direction, [](int index) { return index == 0 ? Orientation::Horizontal : Orientation::Vertical; }},
    Rectangle {fill = colors.red},
    Rectangle {height = 80, fill = colors.blue, rowSpan = 2},
    Rectangle {width = 80, fill = colors.green, columnSpan = 2},
    Rectangle {width = 80, height = 80, fill = rgb(255, 255, 0), columnSpan = 2, rowSpan = 2},
};
auto orientationGroup = RadioButtons {
    header = u"Orientation",
    RadioButton {content = u"Horizontal"},
    RadioButton {content = u"Vertical"},
    selectedIndex = Bind {model->direction},
};
