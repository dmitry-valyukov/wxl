const Preset block {width = 50, height = 50};

auto red = Rectangle {block, fill = colors.red};

auto grid = Grid {
    width = 240,
    height = 160,
    background = colors.gray,
    columnDefinitions = u"50,50,50",
    rowDefinitions = u"50,50,50",
    columnSpacing = 8.0,
    rowSpacing = 8.0,
    red,
    Rectangle {block, fill = colors.blue, row = 1},
    Rectangle {block, fill = colors.green, column = 1},
    Rectangle {block, fill = rgb(255, 255, 0), column = 1, row = 1},
};

auto columnSpacingSlider = Slider {
    row = 1,
    Margin {16, 0, 0, 0},
    header = u"ColumnSpacing",
    minimum = 0.0,
    maximum = 16.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    value = 8.0,
    onValueChanged = [grid](Slider const& self) { grid.columnSpacing(self.value()); },
};

auto rowSpacingSlider = Slider {
    row = 1,
    column = 1,
    orientation.vertical,
    height = 100,
    vAlign.top,
    isDirectionReversed = true,
    header = TextBlock {u"RowSpacing", Margin {0, 0, 0, 10}},
    minimum = 0.0,
    maximum = 16.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    value = 8.0,
    onValueChanged = [grid](Slider const& self) { grid.rowSpacing(self.value()); },
};

auto redColumnSlider = Slider {
    row = 3,
    Margin {16, 0, 0, 0},
    header = u"Grid.Column",
    minimum = 0.0,
    maximum = 2.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    onValueChanged = [red](Slider const& self) { Grid::setColumn(red, static_cast<int>(self.value())); },
};

auto redRowSlider = Slider {
    row = 3,
    column = 1,
    orientation.vertical,
    height = 100,
    vAlign.top,
    isDirectionReversed = true,
    header = TextBlock {u"Grid.Row", Margin {0, 0, 0, 10}},
    minimum = 0.0,
    maximum = 2.0,
    snapsTo = SliderSnapsTo::Ticks,
    stepFrequency = 1.0,
    tickFrequency = 1.0,
    onValueChanged = [red](Slider const& self) { Grid::setRow(red, static_cast<int>(self.value())); },
};

auto options = Grid {
    columnDefinitions = u"auto,auto",
    rowDefinitions = u"auto,auto,auto,auto",
    columnSpacing = 12.0,
    rowSpacing = 12.0,
    TextBlock {u"Grid"},
    columnSpacingSlider,
    rowSpacingSlider,
    TextBlock {u"Red block", row = 2},
    redColumnSlider,
    redRowSlider,
};