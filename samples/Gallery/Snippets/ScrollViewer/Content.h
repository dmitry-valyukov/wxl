// Every option is a field of a model: the viewer shows it, the choice writes it.
struct Model {
    core::observable<int> zoomMode{1};
    core::observable<int> horizontalMode{1};
    core::observable<int> verticalMode{1};
    core::observable<int> horizontalBar{1};
    core::observable<int> verticalBar{1};
    core::observable<double> zoom{4};
};
auto const model = gallery::hold<Model>();

// There's a known issue with zooming where we get into a layout cycle if we
// specify a height but not a width: the size is that of the image.
auto viewer = ScrollViewer {
    width = 400,
    height = 266,
    hAlign.left,
    vAlign.top,
    isTabStop = true,
    isVerticalScrollChainingEnabled = true,
    zoomMode = BindOutput {model->zoomMode, [](int chosen) { return static_cast<ZoomMode>(chosen); }},
    horizontalScrollMode = BindOutput {model->horizontalMode, [](int chosen) { return static_cast<ScrollMode>(chosen); }},
    verticalScrollMode = BindOutput {model->verticalMode, [](int chosen) { return static_cast<ScrollMode>(chosen); }},
    horizontalScrollBarVisibility = BindOutput {model->horizontalBar, [](int chosen) { return static_cast<ScrollBarVisibility>(chosen); }},
    verticalScrollBarVisibility = BindOutput {model->verticalBar, [](int chosen) { return static_cast<ScrollBarVisibility>(chosen); }},
    onLoaded = [model](ScrollViewer const& self) { self.changeView({}, {}, static_cast<float>(model->zoom.get())); },
    // The zoom made with the wheel or a pinch is the zoom of the slider.
    onViewChanged = [model](ScrollViewer const& self, ScrollViewerViewChangedEventArgs& args) {
        if (!args.isIntermediate()) {
            model->zoom.set(self.zoomFactor());
        }
    },
    content = Image {
        hAlign.left,
        vAlign.top,
        toolTip = u"cliff",
        automationName = u"cliff",
        source = u"Assets/SampleMedia/cliff.jpg",
        stretch = Stretch::None,
    },
};

auto caption = [](char16_t const* words, int line, Thickness around, bool centered) {
    return TextBlock {
        row = line,
        margin = around,
        centered ? HorizontalAlignment::Center : HorizontalAlignment::Left,
        vAlign.center,
        columnSpan = centered ? 2 : 1,
        words,
    };
};

// The names of a choice are its rows; its position is the value of the enumeration.
auto choice = [](int line, Thickness around, char16_t const* description, std::initializer_list<char16_t const*> names,
                 core::observable<int>& field) {
    auto box = ComboBox {row = line, column = 1, margin = around, hAlign.stretch, toolTip = description, automationName = description, selectedIndex = Bind {field}};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

auto options = Grid {
    minWidth = 200,
    columnDefinitions = u"auto,*",
    rowDefinitions = u"auto,auto,auto,auto,auto,auto,auto,auto",
    caption(u"ZoomMode", 0, Thickness {0, 0, 10, 0}, false),
    choice(0, Thickness {}, u"zoom mode", {u"Disabled", u"Enabled"}, model->zoomMode),
    Slider {
        row = 1,
        columnSpan = 2,
        Margin {0, 10, 0, 0},
        header = u"Zoom",
        minimum = 0.1,
        maximum = 10.0,
        isEnabled = BindOutput {model->zoomMode, [](int chosen) { return chosen == 1; }},
        value = Bind {model->zoom},
        onValueChanged = [viewer](Slider const& self) { viewer.changeView({}, {}, static_cast<float>(self.value())); },
    },
    caption(u"ScrollMode", 2, Thickness {0, 12}, true),
    caption(u"Horizontal", 3, Thickness {0, 0, 10, 0}, false),
    choice(3, Thickness {}, u"horizontal scroll mode", {u"Disabled", u"Enabled", u"Auto"}, model->horizontalMode),
    caption(u"Vertical", 4, Thickness {0, 8, 10, 0}, false),
    choice(4, Thickness {0, 8, 0, 0}, u"vertical scroll mode", {u"Disabled", u"Enabled", u"Auto"}, model->verticalMode),
    caption(u"ScrollbarVisibility", 5, Thickness {0, 20, 0, 12}, true),
    caption(u"Horizontal", 6, Thickness {0, 0, 10, 0}, false),
    choice(6, Thickness {}, u"horizontal scroll bar visibility", {u"Disabled", u"Auto", u"Hidden", u"Visible"}, model->horizontalBar),
    caption(u"Vertical", 7, Thickness {0, 8, 10, 0}, false),
    choice(7, Thickness {0, 8, 0, 0}, u"vertical scroll bar visibility", {u"Disabled", u"Auto", u"Hidden", u"Visible"}, model->verticalBar),
};