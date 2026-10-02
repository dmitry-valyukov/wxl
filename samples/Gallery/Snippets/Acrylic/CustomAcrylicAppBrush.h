auto behind = [] {
    return Grid {
        Rectangle {width = 100, height = 200, hAlign.left, vAlign.top, fill = rgb(0, 255, 255)},
        Ellipse {width = 152, height = 152, hAlign.center, vAlign.center, fill = rgb(255, 0, 255)},
        Rectangle {width = 80, height = 100, hAlign.right, vAlign.bottom, fill = rgb(255, 255, 0)},
    };
};

// A swatch and its name, as a list row.
auto swatch = [](Color color, char16_t const* name) {
    return ComboBoxItem {
        content = StackPanel {
            orientation.horizontal,
            Rectangle {width = 20, height = 20, fill = color},
            TextBlock {Margin {4, 0, 0, 0}, name},
        },
    };
};

auto brush = AcrylicBrush {tintColor = colors.black, tintOpacity = 0.8, fallbackColor = colors.green};

auto example = Grid {
    minWidth = 320,
    minHeight = 252,
    columnDefinitions = u"*,252",
    behind(),
    Rectangle {Margin {12}, fill = brush},
};

auto options = StackPanel {
    TextBlock {Margin {0, 0, 0, 12}, u"Tint Opacity :"},
    Slider {
        width = 200,
        hAlign.left,
        automationName = u"tint opacity",
        isFocusEngagementEnabled = false,
        minimum = 0.0,
        maximum = 1.0,
        smallChange = 0.001,
        stepFrequency = 0.001,
        value = 0.8,
        onValueChanged = [brush](Slider const& self) { brush.tintOpacity(self.value()); },
    },
    TextBlock {Margin {0, 12}, u"Tint Color :"},
    ComboBox {
        automationName = u"tint color",
        swatch(colors.black, u"#FF000000"),
        swatch(colors.red, u"#FFFF0000"),
        swatch(colors.blue, u"#FF0000FF"),
        selectedIndex = 0,
        onSelectionChanged = [brush](ComboBox const& self) {
            static constexpr Color tints[] = {colors.black, colors.red, colors.blue};
            brush.tintColor(tints[self.selectedIndex()]);
        },
    },
    TextBlock {Margin {0, 12, 0, 12}, u"Fallback Color :"},
    ComboBox {
        automationName = u"fallback color",
        swatch(colors.green, u"#FF008000"),
        swatch(rgb(255, 255, 0), u"#FFFFFF00"),
        selectedIndex = 0,
        onSelectionChanged = [brush](ComboBox const& self) {
            static constexpr Color fallbacks[] = {colors.green, rgb(255, 255, 0)};
            brush.fallbackColor(fallbacks[self.selectedIndex()]);
        },
    },
};