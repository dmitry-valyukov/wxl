// A controller is an object of its own, given the window and a configuration: the policy
// it follows -- whether the window takes input and which theme it is in.
struct Model {
    enum class Backdrop { None, Mica, MicaAlt };

    TextBlock status {hAlign.center};
    ComboBox backdropBox {
        hAlign.stretch,
        header = u"Current backdrop ",
        ComboBoxItem {content = u"Mica"},
        ComboBoxItem {content = u"MicaAlt"},
        ComboBoxItem {content = u"None"},
        selectedIndex = 0,
    };
    ComboBox themeBox {
        hAlign.stretch,
        header = u"Window theme ",
        ComboBoxItem {content = u"Use system setting"},
        ComboBoxItem {content = u"Light"},
        ComboBoxItem {content = u"Dark"},
        selectedIndex = 0,
    };
    Grid root {
        rowDefinitions = u"32,*",
        TitleBar {
            title = u"SystemBackdrop sample window",
            iconSource = ImageIconSource {imageSource = u"Assets/Tiles/BadgeLogo.png"},
        },
        StackPanel {row = 1, hAlign.center, vAlign.center, spacing = 20, status, backdropBox, themeBox},
    };
    Window window {extendsContentIntoTitleBar = true, content = root};

    // The material that is drawn now: one controller at a time.
    std::shared_ptr<MicaController> controller;
    std::shared_ptr<SystemBackdropConfiguration> configuration;
    Backdrop current = Backdrop::None;

    Model() {
        window.appWindow().setIcon(u"Assets/Tiles/GalleryIcon.ico");
        backdropBox.add_onSelectionChanged([this](auto&&...) {
            static constexpr Backdrop kinds[] = {Backdrop::Mica, Backdrop::MicaAlt, Backdrop::None};
            setBackdrop(kinds[std::clamp(backdropBox.selectedIndex(), 0, 2)]);
        });
        themeBox.add_onSelectionChanged([this](auto&&...) {
            static constexpr ElementTheme themes[] = {ElementTheme::Default, ElementTheme::Light, ElementTheme::Dark};
            root.requestedTheme(themes[std::clamp(themeBox.selectedIndex(), 0, 2)]);
            setNoneBackground();
        });
        // The configuration follows the window: whether it is active, and its theme.
        window.add_onActivated([this](Object const&, WindowActivatedEventArgs& args) {
            if (configuration) {
                configuration->isInputActive(args.windowActivationState() != WindowActivationState::Deactivated);
            }
        });
        root.add_onActualThemeChanged([this](auto&&...) { setConfigurationTheme(); });
        // The controller is closed with the window, so that it does not draw under a closed one.
        window.add_onClosed([this](auto&&...) { release(); });
    }

    void release() {
        if (controller) {
            controller->close();
            controller.reset();
        }
        configuration.reset();
    }

    void setConfigurationTheme() {
        if (configuration) {
            configuration->theme(static_cast<SystemBackdropTheme>(root.actualTheme()));
        }
    }

    void setBackdrop(Backdrop type) {
        // The previous controller is taken away completely, to start from the default colour:
        // an application that chooses one controller at start-up does just that.
        current = Backdrop::None;
        status.text(u"");
        release();

        if (type != Backdrop::None) {
            if (MicaController::isSupported()) {
                // Hooking up the policy object.
                configuration = std::make_shared<SystemBackdropConfiguration>();
                configuration->isInputActive(true);
                setConfigurationTheme();

                controller = std::make_shared<MicaController>(
                    MicaController {kind = type == Backdrop::MicaAlt ? MicaKind::BaseAlt : MicaKind::Base});
                // Enable the system backdrop.
                window.micaController(*controller);
                controller->setSystemBackdropConfiguration(*configuration);
                current = type;
            } else {
                // Mica isn't supported on this system.
                status.text(u"  Mica isn't supported.");
            }
        }
        setNoneBackground();
    }

    // Fixes the background colour not changing when switching between themes.
    void setNoneBackground() {
        if (current == Backdrop::None && themeBox.selectedIndex() != 0) {
            root.background(SolidColorBrush {color = themeBox.selectedIndex() == 1 ? colors.white : colors.black});
        } else {
            root.background(SolidColorBrush {color = colors.transparent});
        }
    }
};

auto example = StackPanel {
    RichTextBlock {
        Paragraph {
            Run {u"MicaController provides a customizable way to apply the Mica material. You can modify: "
                 u"FallbackColor, Kind, LuminosityOpacity, TintColor, and TintOpacity."},
            LineBreak {},
            LineBreak {},
            Run {u"There are 2 kinds of Mica:"},
            LineBreak {},
            Run {u"1. "},
            Run {u"Base", FontWeight {700}},
            Run {u" — The default, lighter appearance."},
            LineBreak {},
            Run {u"2. "},
            Run {u"Alt", FontWeight {700}},
            Run {u" — A darker appearance with stronger tinting of the desktop wallpaper."},
        },
    },
    Button {
        Margin {0, 10, 0, 0},
        content = u"Show window",
        onClick = [](Button const&) {
            auto model = std::make_shared<Model>();
            gallery::trackWindow(model->window, model);
            model->window.activate();
            model->setBackdrop(Model::Backdrop::Mica);
        },
    },
};
