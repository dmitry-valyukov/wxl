auto border = Border {
    vAlign.top,
    background = colors.white,
    borderBrush = rgb(255, 215, 0),
    BorderThickness {2},
    TextBlock {u"Text inside a border", Margin {8, 5}, fontSize = 18, foreground = colors.black},
};

auto thickness = Slider {
    header = u"BorderThickness",
    minimum = 0.0,
    maximum = 10.0,
    stepFrequency = 1.0,
    value = 2.0,
    onValueChanged = [border](Slider const& self) { border.borderThickness(Thickness {self.value()}); },
};

auto backgroundChoice = RadioButtons {
    header = u"Background",
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"Blue"},
    RadioButton {content = u"White"},
    selectedIndex = 3,
    onSelectionChanged = [border](RadioButtons const& self) {
        static constexpr Color paints[] = {rgb(0, 128, 0), rgb(255, 255, 0), rgb(0, 0, 255), rgb(255, 255, 255)};
        int const index = self.selectedIndex();
        if (index >= 0) {
            border.background(SolidColorBrush {color = paints[index]});
        }
    },
};

auto brushChoice = RadioButtons {
    header = u"BorderBrush",
    column = 1,
    RadioButton {content = u"Green"},
    RadioButton {content = u"Yellow"},
    RadioButton {content = u"Blue"},
    RadioButton {content = u"White"},
    selectedIndex = 1,
    onSelectionChanged = [border](RadioButtons const& self) {
        static constexpr Color paints[] = {rgb(0, 100, 0), rgb(255, 215, 0), rgb(0, 0, 139), rgb(255, 255, 255)};
        int const index = self.selectedIndex();
        if (index >= 0) {
            border.borderBrush(SolidColorBrush {color = paints[index]});
        }
    },
};