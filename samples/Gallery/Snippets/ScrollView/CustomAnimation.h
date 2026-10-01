struct Model {
    core::observable<int> animation{0};
    core::observable<double> duration{1500};
};
auto const model = gallery::hold<Model>();

auto picture = [](char16_t const* name, char16_t const* path) {
    return Image {toolTip = name, source = path, stretch = Stretch::Uniform};
};

// Down to the fifth of the way, or up to four fifths of it: the other end of the two.
auto targetOffset = [](ScrollView const& view) {
    return view.verticalOffset() > view.scrollableHeight() / 2.0 ? view.scrollableHeight() / 5.0 : 4.0 * view.scrollableHeight() / 5.0;
};

auto view = ScrollView {
    width = 400,
    height = 300,
    hAlign.left,
    vAlign.top,
    isTabStop = true,
    // The scroll is about to run its stock animation; the handler may run its own instead.
    onScrollAnimationStarting = [model, targetOffset](ScrollView const& self, ScrollingScrollAnimationStartingEventArgs& args) {
        auto const stock = args.animation().try_as<Vector3KeyFrameAnimation>();
        auto const duration = core::duration {std::chrono::milliseconds {static_cast<int64_t>(model->duration.get())}};
        if (model->animation.get() == 0) {
            stock.duration(duration);
            return;
        }

        auto const compositor = stock.compositor();
        auto const custom = compositor.createVector3KeyFrameAnimation();
        auto const target = static_cast<float>(targetOffset(self));
        auto const here = static_cast<float>(self.horizontalOffset());
        auto const travel = static_cast<float>(target - self.verticalOffset());

        if (model->animation.get() == 1) {
            // Accordion: overshoots, and comes back in smaller and smaller swings.
            float swing = 0.1f * travel;
            for (int step = 0; step < 3; ++step) {
                custom.insertKeyFrame(1.0f - 0.4f / static_cast<float>(1 << step), Vector3 {here, target + swing, 0.0f});
                swing /= -2.0f;
            }
            custom.insertKeyFrame(1.0f, Vector3 {here, target, 0.0f});
        } else {
            // Teleportation: rushes off, jumps the gap in the middle, and rushes in.
            custom.insertKeyFrame(0.499999f, Vector3 {here, target - 0.9f * travel, 0.0f},
                                  compositor.createCubicBezierEasingFunction(Vector2 {1.0f, 0.0f}, Vector2 {1.0f, 0.0f}));
            custom.insertKeyFrame(0.5f, Vector3 {here, target - 0.1f * travel, 0.0f},
                                  StepEasingFunction::createStepEasingFunction(compositor, 1));
            custom.insertKeyFrame(1.0f, Vector3 {here, target, 0.0f},
                                  compositor.createCubicBezierEasingFunction(Vector2 {0.0f, 1.0f}, Vector2 {0.0f, 1.0f}));
        }
        custom.duration(duration);
        args.animation(custom);
    },
    content = StackPanel {
        picture(u"leaves", u"Assets/SampleMedia/LandscapeImage1.jpg"),
        picture(u"carousel", u"Assets/SampleMedia/LandscapeImage2.jpg"),
        picture(u"bicycles", u"Assets/SampleMedia/LandscapeImage3.jpg"),
        picture(u"pond", u"Assets/SampleMedia/LandscapeImage4.jpg"),
        picture(u"marina", u"Assets/SampleMedia/LandscapeImage5.jpg"),
        picture(u"beach", u"Assets/SampleMedia/LandscapeImage6.jpg"),
        picture(u"rampart", u"Assets/SampleMedia/LandscapeImage7.jpg"),
        picture(u"mountain", u"Assets/SampleMedia/LandscapeImage8.jpg"),
    },
};

auto example = StackPanel {
    spacing = 16.0,
    TextBlock {
        u"Pick an animation type and its duration and then click the button on the right to launch a programmatic scroll.",
        textWrapping = TextWrapping::Wrap,
    },
    view,
};

auto options = Grid {
    minWidth = 320,
    columnDefinitions = u"auto,*",
    columnSpacing = 12.0,
    rowDefinitions = u"auto,auto,auto",
    rowSpacing = 16.0,
    TextBlock {vAlign.center, u"Scroll with animation"},
    ComboBox {
        column = 1,
        hAlign.stretch,
        toolTip = u"vertical animation options",
        ComboBoxItem {content = u"Default"},
        ComboBoxItem {content = u"Accordion"},
        ComboBoxItem {content = u"Teleportation"},
        selectedIndex = Bind {model->animation},
    },
    TextBlock {row = 1, vAlign.center, u"Animation duration (msec)"},
    NumberBox {
        row = 1,
        column = 1,
        toolTip = u"animation duration",
        largeChange = 1000.0,
        maximum = 5000.0,
        minimum = 1000.0,
        smallChange = 500.0,
        spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
        value = Bind {model->duration},
    },
    Button {
        row = 2,
        columnSpan = 2,
        hAlign.stretch,
        toolTip = u"scroll with animation",
        content = u"Scroll with animation",
        onClick = [view, targetOffset] {
            view.scrollTo(view.horizontalOffset(), targetOffset(view),
                          ScrollingScrollOptions {ScrollingAnimationMode::Enabled, ScrollingSnapPointsMode::Ignore});
        },
    },
};