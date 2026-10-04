auto picture = [](char16_t const* name, char16_t const* path) {
    return Image {toolTip = name, automationName = name, source = path, stretch = Stretch::Uniform};
};

auto view = ScrollView {
    width = 400,
    height = 300,
    hAlign.left,
    vAlign.top,
    isTabStop = true,
    content = StackPanel {
        picture(u"grapes", u"Assets/SampleMedia/grapes.jpg"),
        picture(u"rainier", u"Assets/SampleMedia/rainier.jpg"),
        picture(u"sunset", u"Assets/SampleMedia/sunset.jpg"),
        picture(u"treetops", u"Assets/SampleMedia/treetops.jpg"),
        picture(u"valley", u"Assets/SampleMedia/valley.jpg"),
        picture(u"cliff", u"Assets/SampleMedia/cliff.jpg"),
    },
};

auto example = StackPanel {
    spacing = 16.0,
    TextBlock {
        u"Set the vertical velocity to a value greater than 30 to scroll down, "
        u"or a value smaller than -30 to scroll up at a constant speed.",
        textWrapping = TextWrapping::Wrap,
    },
    view,
};

auto options = Grid {
    minWidth = 200,
    columnDefinitions = u"auto,*",
    columnSpacing = 12.0,
    TextBlock {vAlign.center, u"Vertical velocity"},
    NumberBox {
        column = 1,
        toolTip = u"vertical velocity",
        automationName = u"vertical velocity",
        largeChange = 30.0,
        maximum = 200.0,
        minimum = -200.0,
        smallChange = 10.0,
        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
        value = 30.0,
        onValueChanged = [view](NumberBox const& self, NumberBoxValueChangedEventArgs& args) {
            if (std::isnan(args.oldValue())) {
                return;
            }
            // Cancel the constant velocity there was.
            view.scrollBy(0, 0, ScrollingScrollOptions {ScrollingAnimationMode::Disabled, ScrollingSnapPointsMode::Ignore});

            // Only a value smaller than -30 or greater than 30 starts a scroll; a value between
            // them turns the scroll round, or sets it going when the view is at an end.
            auto velocity = static_cast<float>(self.value());
            auto const atTop = view.verticalOffset() == 0;
            auto const atBottom = view.verticalOffset() == view.scrollableHeight();
            if (args.newValue() <= 30.0 && args.newValue() >= -30) {
                if (args.newValue() < args.oldValue()) {
                    velocity = atTop ? 30 : -30;
                } else {
                    velocity = atBottom ? -30 : 30;
                }
            } else if (args.newValue() < 30.0 && atTop) {
                velocity = 30;
            } else if (args.newValue() > 30.0 && atBottom) {
                velocity = -30;
            }

            self.value(velocity);
            // No decay rate: the velocity is constant.
            view.addScrollVelocity(Vector2 {0, velocity}, Vector2 {});
        },
    },
};