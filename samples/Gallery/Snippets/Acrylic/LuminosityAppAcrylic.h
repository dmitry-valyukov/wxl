auto behind = [] {
    return Grid {
        Rectangle {width = 100, height = 200, hAlign.left, vAlign.top, fill = rgb(0, 255, 255)},
        Ellipse {width = 152, height = 152, hAlign.center, vAlign.center, fill = rgb(255, 0, 255)},
        Rectangle {width = 80, height = 100, hAlign.right, vAlign.bottom, fill = rgb(255, 255, 0)},
    };
};

constexpr Color skyBlue = rgb(0x87, 0xce, 0xeb);
auto brush = AcrylicBrush {tintColor = skyBlue, tintOpacity = 0.8, tintLuminosityOpacity = 0.8, fallbackColor = skyBlue};

auto example = Grid {
    minWidth = 652,
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
    TextBlock {Margin {0, 12, 0, 12}, u"Tint Luminosity Opacity :"},
    Slider {
        width = 200,
        hAlign.left,
        automationName = u"tint luminosity",
        isFocusEngagementEnabled = false,
        minimum = 0.0,
        maximum = 1.0,
        smallChange = 0.001,
        stepFrequency = 0.001,
        value = 0.8,
        onValueChanged = [brush](Slider const& self) { brush.tintLuminosityOpacity(self.value()); },
    },
};