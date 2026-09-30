const Preset square {width = 40, height = 40};

auto red = Rectangle {square, fill = colors.red};

auto canvas = Canvas {
    width = 140,
    height = 140,
    vAlign.top,
    background = colors.gray,
    red,
    Rectangle {square, fill = colors.blue, left = 20.0, top = 20.0, zIndex = 1},
    Rectangle {square, fill = colors.green, left = 40.0, top = 40.0, zIndex = 2},
    Rectangle {square, fill = rgb(255, 255, 0), left = 60.0, top = 60.0, zIndex = 3},
};

auto topSlider = Slider {
    orientation.vertical,
    height = 110,
    vAlign.top,
    isDirectionReversed = true,
    header = TextBlock {u"Canvas.Top", Margin {0, 0, 0, 10}},
    minimum = 0.0,
    maximum = 100.0,
    stepFrequency = 1.0,
    onValueChanged = [red](Slider const& self) { Canvas::setTop(red, self.value()); },
};

auto leftSlider = Slider {
    width = 100,
    header = u"Canvas.Left",
    minimum = 0.0,
    maximum = 100.0,
    stepFrequency = 1.0,
    onValueChanged = [red](Slider const& self) { Canvas::setLeft(red, self.value()); },
};

auto zSlider = Slider {
    width = 100,
    header = u"Canvas.ZIndex",
    minimum = 0.0,
    maximum = 4.0,
    stepFrequency = 1.0,
    onValueChanged = [red](Slider const& self) { Canvas::setZIndex(red, static_cast<int>(self.value())); },
};