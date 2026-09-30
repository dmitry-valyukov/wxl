static constexpr const char16_t* fonts[] = {
    u"Arial", u"Comic Sans MS", u"Courier New", u"Segoe UI", u"Times New Roman",
};

auto output = TextBlock {
    u"You can set the font used for this text.",
    fontFamily = fonts[2],
    styles.TextBlock.Body,
    Margin {0, 8, 0, 0},
};

auto combo = ComboBox {
    minWidth = 200,
    header = u"Font",
    ComboBoxItem {content = fonts[0]},
    ComboBoxItem {content = fonts[1]},
    ComboBoxItem {content = fonts[2]},
    ComboBoxItem {content = fonts[3]},
    ComboBoxItem {content = fonts[4]},
    selectedIndex = 2,
    onSelectionChanged = [output](ComboBox const& self) {
        int const index = self.selectedIndex();
        if (index >= 0) {
            output.fontFamily(fonts[index]);
        }
    },
};