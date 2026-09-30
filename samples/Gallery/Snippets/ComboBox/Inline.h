auto output = Rectangle {width = 100, height = 30, Margin {0, 8, 0, 0}};

auto combo = ComboBox {
    width = 200,
    header = u"Colors",
    placeholderText = u"Pick a color",
    ComboBoxItem {content = u"Blue"},
    ComboBoxItem {content = u"Green"},
    ComboBoxItem {content = u"Red"},
    ComboBoxItem {content = u"Yellow"},
    onSelectionChanged = [output](ComboBox const& self) {
        static constexpr Color colors[] = {rgb(0, 0, 255), rgb(0, 128, 0), rgb(255, 0, 0), rgb(255, 255, 0)};
        int const index = self.selectedIndex();
        if (index >= 0) {
            output.fill(SolidColorBrush {color = colors[index]});
        }
    },
};