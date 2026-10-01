// Twelve colours of the title bar of a window. Each is a field of the model: the
// pickers write them, the window reads them when it opens, and a change made while it
// is open goes straight to its title bar.
struct Model {
    core::observable<Color> background {rgb(0xf2, 0xf6, 0xfa)};
    core::observable<Color> foreground {rgb(0x1e, 0x29, 0x33)};
    core::observable<Color> buttonBackground {rgb(0x3b, 0x82, 0xf6)};
    core::observable<Color> buttonForeground {rgb(0xff, 0xff, 0xff)};
    core::observable<Color> buttonHoverBackground {rgb(0x25, 0x63, 0xeb)};
    core::observable<Color> buttonHoverForeground {rgb(0xff, 0xff, 0xff)};
    core::observable<Color> inactiveBackground {rgb(0xe5, 0xea, 0xf0)};
    core::observable<Color> inactiveForeground {rgb(0x6b, 0x72, 0x80)};
    core::observable<Color> buttonInactiveBackground {rgb(0xcb, 0xd5, 0xe1)};
    core::observable<Color> buttonInactiveForeground {rgb(0x47, 0x55, 0x69)};
    core::observable<Color> buttonPressedBackground {rgb(0x1d, 0x4e, 0xd8)};
    core::observable<Color> buttonPressedForeground {rgb(0xff, 0xff, 0xff)};

    // What each colour sets on the title bar.
    struct Channel {
        core::observable<Color>& color;
        void (*apply)(AppWindowTitleBar const&, Color);
    };
    Channel channels[12] = {
        {background, [](AppWindowTitleBar const& bar, Color value) { bar.backgroundColor(value); }},
        {foreground, [](AppWindowTitleBar const& bar, Color value) { bar.foregroundColor(value); }},
        {buttonBackground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonBackgroundColor(value); }},
        {buttonForeground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonForegroundColor(value); }},
        {buttonHoverBackground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonHoverBackgroundColor(value); }},
        {buttonHoverForeground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonHoverForegroundColor(value); }},
        {inactiveBackground, [](AppWindowTitleBar const& bar, Color value) { bar.inactiveBackgroundColor(value); }},
        {inactiveForeground, [](AppWindowTitleBar const& bar, Color value) { bar.inactiveForegroundColor(value); }},
        {buttonInactiveBackground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonInactiveBackgroundColor(value); }},
        {buttonInactiveForeground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonInactiveForegroundColor(value); }},
        {buttonPressedBackground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonPressedBackgroundColor(value); }},
        {buttonPressedForeground, [](AppWindowTitleBar const& bar, Color value) { bar.buttonPressedForegroundColor(value); }},
    };

    Button showButton {content = u"Show window"};
    std::shared_ptr<Window> window;
    std::weak_ptr<Model> self;

    Model() {
        for (auto& channel : channels) {
            channel.color.on_change([this, &channel](auto&&...) noexcept {
                if (window) {
                    channel.apply(window->appWindow().titleBar(), channel.color.get());
                }
            });
        }
    }

    void open() {
        showButton.isEnabled(false);
        window = std::make_shared<Window>(Window {
            systemBackdrop = MicaBackdrop {},
            content = Grid {
                TextBlock {
                    u"This is a sample window to demonstrate AppWindowTitleBar color customization.",
                    hAlign.center,
                    vAlign.center,
                    textWrapping = TextWrapping::Wrap,
                    Margin {20},
                    fontSize = 16,
                },
            },
        });

        auto presenter = OverlappedPresenter::create();
        presenter.isAlwaysOnTop(true);
        presenter.isResizable(false);
        window->appWindow().setPresenter(presenter);
        window->appWindow().resize({600, 400});
        for (auto& channel : channels) {
            channel.apply(window->appWindow().titleBar(), channel.color.get());
        }

        window->add_onClosed([weak = self](auto&&...) {
            if (auto const model = weak.lock()) {
                model->showButton.isEnabled(true);
                model->window.reset();
            }
        });
        gallery::trackWindow(*window);
        window->activate();
    }
};
auto const model = gallery::hold<Model>();
model->self = model;
model->showButton.add_onClick([weak = model->self](auto&&...) {
    if (auto const model = weak.lock()) {
        model->open();
    }
});

auto example = model->showButton;

auto options = Grid {
    columnSpacing = 16,
    columnDefinitions = u"auto,auto,auto",
    // Normal and hover states.
    StackPanel {
        column = 0,
        spacing = 8,
        TextBlock {u"BackgroundColor"},
        gallery::colorSelector(model->background, u"BackgroundColor"),
        TextBlock {u"ForegroundColor"},
        gallery::colorSelector(model->foreground, u"ForegroundColor"),
        TextBlock {u"ButtonBackgroundColor"},
        gallery::colorSelector(model->buttonBackground, u"ButtonBackgroundColor"),
        TextBlock {u"ButtonForegroundColor"},
        gallery::colorSelector(model->buttonForeground, u"ButtonForegroundColor"),
        TextBlock {u"ButtonHoverBackgroundColor"},
        gallery::colorSelector(model->buttonHoverBackground, u"ButtonHoverBackgroundColor"),
        TextBlock {u"ButtonHoverForegroundColor"},
        gallery::colorSelector(model->buttonHoverForeground, u"ButtonHoverForegroundColor"),
    },
    AppBarSeparator {column = 1},
    // Inactive and pressed states.
    StackPanel {
        column = 2,
        spacing = 8,
        TextBlock {u"InactiveBackgroundColor"},
        gallery::colorSelector(model->inactiveBackground, u"InactiveBackgroundColor"),
        TextBlock {u"InactiveForegroundColor"},
        gallery::colorSelector(model->inactiveForeground, u"InactiveForegroundColor"),
        TextBlock {u"ButtonInactiveBackgroundColor"},
        gallery::colorSelector(model->buttonInactiveBackground, u"ButtonInactiveBackgroundColor"),
        TextBlock {u"ButtonInactiveForegroundColor"},
        gallery::colorSelector(model->buttonInactiveForeground, u"ButtonInactiveForegroundColor"),
        TextBlock {u"ButtonPressedBackgroundColor"},
        gallery::colorSelector(model->buttonPressedBackground, u"ButtonPressedBackgroundColor"),
        TextBlock {u"ButtonPressedForegroundColor"},
        gallery::colorSelector(model->buttonPressedForeground, u"ButtonPressedForegroundColor"),
    },
};