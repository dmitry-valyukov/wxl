auto grid = VariableSizedWrapGrid {
    width = 400,
    itemHeight = 44,
    itemWidth = 44,
    maximumRowsOrColumns = 3,
    Rectangle {fill = colors.red},
    Rectangle {height = 80, fill = colors.blue, rowSpan = 2},
    Rectangle {width = 80, fill = colors.green, columnSpan = 2},
    Rectangle {width = 80, height = 80, fill = rgb(255, 255, 0), columnSpan = 2, rowSpan = 2},
};
auto orientationGroup = RadioButtons {
    header = u"Orientation",
    RadioButton {content = u"Horizontal"},
    RadioButton {content = u"Vertical"},
    selectedIndex = 1,
    onSelectionChanged = [grid](RadioButtons const& self) {
        int const index = self.selectedIndex();
        if (index >= 0) {
            grid.orientation(index == 0 ? Orientation::Horizontal : Orientation::Vertical);
        }
    },
};