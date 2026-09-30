auto chooser = SplitButton {
    minWidth = 0,
    minHeight = 0,
    Padding {5},
    vAlign.top,
    content = u"Choose color",
};

const Preset dot {width = 32, height = 32, radiusX = 4.0, radiusY = 4.0};

const Preset cell {
    Padding {0},
    minWidth = 0,
    minHeight = 0,
    Margin {6},
    CornerRadius {4},
    onClick = [chooser] { chooser.flyout().hide(); },
};

chooser.flyout(Flyout {
    placement = FlyoutPlacementMode::Bottom,
    content = VariableSizedWrapGrid {
        maximumRowsOrColumns = 3,
        orientation.horizontal,
        Button {cell, content = Rectangle {dot, fill = rgb(255, 0, 0)}},
        Button {cell, content = Rectangle {dot, fill = rgb(255, 165, 0)}},
        Button {cell, content = Rectangle {dot, fill = rgb(255, 255, 0)}},
        Button {cell, content = Rectangle {dot, fill = rgb(0, 128, 0)}},
        Button {cell, content = Rectangle {dot, fill = rgb(0, 0, 255)}},
        Button {cell, content = Rectangle {dot, fill = rgb(75, 0, 130)}},
        Button {cell, content = Rectangle {dot, fill = rgb(238, 130, 238)}},
        Button {cell, content = Rectangle {dot, fill = rgb(128, 128, 128)}},
        Button {cell, content = Rectangle {dot, fill = rgb(0, 0, 0)}},
    },
});