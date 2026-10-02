auto sample = Border {
    height = 50,
    Margin {0, 10, 0, 10},
    background = rgb(0, 128, 0),
    borderBrush = rgb(255, 215, 0),
    BorderThickness {10},
};

auto background = RadioButtons {
    header = u"Background",
    maxColumns = 3,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"White"},
    selectedIndex = 0,
    onSelectionChanged = [sample](RadioButtons const& self) {
        static constexpr Color colors[] = {rgb(0, 128, 0), rgb(255, 255, 0), rgb(255, 255, 255)};
        int const index = self.selectedIndex();
        if (index >= 0) {
            sample.background(SolidColorBrush {color = colors[index]});
        }
    },
};

auto border = RadioButtons {
    header = u"Border",
    maxColumns = 3,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"White"},
    selectedIndex = 1,
    onSelectionChanged = [sample](RadioButtons const& self) {
        static constexpr Color colors[] = {rgb(0, 100, 0), rgb(255, 215, 0), rgb(255, 255, 255)};
        int const index = self.selectedIndex();
        if (index >= 0) {
            sample.borderBrush(SolidColorBrush {color = colors[index]});
        }
    },
};