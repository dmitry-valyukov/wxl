// Every option is a field of a model: the view shows it, the choice writes it.
struct Model {
    core::observable<int> zoomMode{0};
    core::observable<int> horizontalMode{2};
    core::observable<int> verticalMode{2};
    core::observable<int> horizontalBar{0};
    core::observable<int> verticalBar{0};
};
auto const model = gallery::hold<Model>();

auto view = ScrollView {
    width = 400,
    height = 266,
    hAlign.left,
    vAlign.top,
    contentOrientation = ScrollingContentOrientation::None,
    isTabStop = true,
    zoomMode = BindOutput {model->zoomMode, [](int chosen) { return static_cast<ScrollingZoomMode>(chosen); }},
    horizontalScrollMode = BindOutput {model->horizontalMode, [](int chosen) { return static_cast<ScrollingScrollMode>(chosen); }},
    verticalScrollMode = BindOutput {model->verticalMode, [](int chosen) { return static_cast<ScrollingScrollMode>(chosen); }},
    horizontalScrollBarVisibility = BindOutput {model->horizontalBar, [](int chosen) { return static_cast<ScrollingScrollBarVisibility>(chosen); }},
    verticalScrollBarVisibility = BindOutput {model->verticalBar, [](int chosen) { return static_cast<ScrollingScrollBarVisibility>(chosen); }},
    // The view starts zoomed in, reaching that with an animation.
    onLoaded = [](ScrollView const& self) {
        self.zoomTo(4.0f, {}, ScrollingZoomOptions {ScrollingAnimationMode::Enabled, ScrollingSnapPointsMode::Ignore});
    },
    content = Image {
        hAlign.center,
        vAlign.center,
        toolTip = u"cliff",
        automationName = u"cliff",
        source = u"Assets/SampleMedia/cliff.jpg",
        stretch = Stretch::Uniform,
    },
};

auto example = StackPanel {
    spacing = 16.0,
    TextBlock {
        u"This ScrollView allows horizontal and vertical scrolling, as well as zooming. "
        u"Change the settings on the right to alter those capabilities or the built-in scrollbars' visibility.",
        textWrapping = TextWrapping::Wrap,
    },
    view,
};

auto caption = [](char16_t const* words, int line, bool centered) {
    return TextBlock {row = line, vAlign.center, columnSpan = centered ? 2 : 1, words, centered ? HorizontalAlignment::Center : HorizontalAlignment::Left};
};

// The names of a choice are its rows; its position is the value of the enumeration.
auto choice = [](int line, char16_t const* description, std::initializer_list<char16_t const*> names, core::observable<int>& field) {
    auto box = ComboBox {row = line, column = 1, hAlign.stretch, toolTip = description, automationName = description, selectedIndex = Bind {field}};
    for (auto const name : names) {
        box.items().append(ComboBoxItem {content = name});
    }
    return box;
};

auto formatter = DecimalFormatter {
    integerDigits = 2,
    fractionDigits = 1,
    numberRounder = IncrementNumberRounder {increment = 0.1, roundingAlgorithm = RoundingAlgorithm::RoundHalfUp},
};

auto options = Grid {
    minWidth = 200,
    columnDefinitions = u"auto,*",
    columnSpacing = 12.0,
    rowDefinitions = u"auto,auto,auto,auto,auto,auto,auto,auto",
    rowSpacing = 16.0,
    caption(u"ZoomMode", 0, false),
    choice(0, u"zoom mode", {u"Enabled", u"Disabled"}, model->zoomMode),
    caption(u"ZoomFactor", 1, false),
    NumberBox {
        row = 1,
        column = 1,
        toolTip = u"zoom factor",
        automationName = u"zoom factor",
        largeChange = 10.0,
        maximum = 10.0,
        minimum = 0.1,
        smallChange = 1.0,
        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
        numberFormatter = formatter,
        value = 4.0,
        onValueChanged = [view](NumberBox const&, NumberBoxValueChangedEventArgs& args) { view.zoomTo(static_cast<float>(args.newValue()), {}); },
    },
    caption(u"ScrollMode", 2, true),
    caption(u"Horizontal", 3, false),
    choice(3, u"horizontal scroll mode", {u"Enabled", u"Disabled", u"Auto"}, model->horizontalMode),
    caption(u"Vertical", 4, false),
    choice(4, u"vertical scroll mode", {u"Enabled", u"Disabled", u"Auto"}, model->verticalMode),
    caption(u"ScrollbarVisibility", 5, true),
    caption(u"Horizontal", 6, false),
    choice(6, u"horizontal scroll bar visibility", {u"Auto", u"Visible", u"Hidden"}, model->horizontalBar),
    caption(u"Vertical", 7, false),
    choice(7, u"vertical scroll bar visibility", {u"Auto", u"Visible", u"Hidden"}, model->verticalBar),
};