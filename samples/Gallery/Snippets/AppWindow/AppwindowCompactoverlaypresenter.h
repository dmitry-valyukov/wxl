struct Model {
    ComboBox initialSize {
        width = 150,
        header = u"InitialSize",
        ComboBoxItem {content = u"Small"},
        ComboBoxItem {content = u"Medium"},
        ComboBoxItem {content = u"Large"},
        selectedIndex = 0,
    };
    TextBlock description {width = 250, textWrapping = TextWrapping::Wrap, u"Small: Window size is approximately 5% of the display's work area."};

    Model() {
        initialSize.add_onSelectionChanged([this](auto&&...) {
            static constexpr char16_t const* texts[] = {
                u"Small: Window size is approximately 5% of the display's work area.",
                u"Medium: Window size is approximately 15% of the display's work area.",
                u"Large: Window size is approximately 25% of the display's work area."};
            description.text(texts[std::clamp(initialSize.selectedIndex(), 0, 2)]);
        });
    }

    void open() {
        Window window {
            systemBackdrop = MicaBackdrop {},
            content = StackPanel {
                hAlign.center,
                vAlign.center,
                spacing = 8,
                TextBlock {
                    textAlignment = TextAlignment::Center,
                    textWrapping = TextWrapping::Wrap,
                    u"This window is set to CompactOverlay (Picture-in-Picture) mode.",
                },
            },
        };
        auto appWindow = window.appWindow();
        appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
        appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);

        auto presenter = CompactOverlayPresenter::create();
        static constexpr CompactOverlaySize sizes[] = {
            CompactOverlaySize::Small, CompactOverlaySize::Medium, CompactOverlaySize::Large};
        presenter.initialSize(sizes[std::clamp(initialSize.selectedIndex(), 0, 2)]);
        appWindow.setPresenter(presenter);

        gallery::trackWindow(window);
        window.activate();
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"CompactOverlayPresenter (Picture-in-Picture mode) keeps an AppWindow always on top while using minimal "
        u"screen space. To ensure a good user experience, the window should have a small yet functional size (e.g., "
        u"for media players or floating tools).",
    },
    Button {content = u"Show window (Picture-in-Picture mode)", onClick = [model](Button const&) { model->open(); }},
};

auto options = StackPanel {spacing = 8, model->initialSize, model->description};