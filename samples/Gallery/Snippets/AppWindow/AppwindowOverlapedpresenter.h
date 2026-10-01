struct Model {
    // A switch with the explanation that stands on it as a tip.
    static ToggleSwitch option(char16_t const* label, char16_t const* tip, bool on) {
        return ToggleSwitch {header = label, isOn = on, offContent = u"false", onContent = u"true", toolTip = tip};
    }

    ToggleSwitch alwaysOnTop = option(u"IsAlwaysOnTop", u"Keeps the window above all other windows when enabled.", false);
    ToggleSwitch maximizable = option(u"IsMaximizable", u"Controls the visibility and enable state of the maximize button.", true);
    ToggleSwitch minimizable = option(u"IsMinimizable", u"Controls the visibility and enable state of the minimize button.", true);
    ToggleSwitch resizable = option(u"IsResizable", u"Allows resizing the window by dragging its edges.", true);
    ToggleSwitch hasBorder = option(u"HasBorder", u"Determines if the window has a visible outer border.", true);
    ToggleSwitch hasTitleBar = option(u"HasTitleBar", u"Specifies whether the window includes a system title bar.", true);

    Model() {
        // A title bar needs a border: asking for a window with the one and not the other is
        // a fatal error of the framework, so the two switches keep each other honest.
        hasBorder.add_onToggled([this](auto&&...) {
            if (!hasBorder.isOn()) {
                hasTitleBar.isOn(false);
            }
        });
        hasTitleBar.add_onToggled([this](auto&&...) {
            if (hasTitleBar.isOn()) {
                hasBorder.isOn(true);
            }
        });
    }

    void open() {
        Button maximizeButton {width = 200, content = u"Maximize"};
        Button minimizeButton {width = 200, content = u"Minimize"};
        Button restoreButton {width = 200, content = u"Minimize and restore after 3 seconds"};
        Button closeButton {width = 200, content = u"Close"};
        Window window {
            systemBackdrop = MicaBackdrop {},
            content = StackPanel {
                hAlign.center, vAlign.center, spacing = 10, maximizeButton, minimizeButton, restoreButton, closeButton},
        };
        auto appWindow = window.appWindow();

        auto presenter = OverlappedPresenter::create();
        presenter.isAlwaysOnTop(alwaysOnTop.isOn());
        presenter.isMaximizable(maximizable.isOn());
        presenter.isMinimizable(minimizable.isOn());
        presenter.isResizable(resizable.isOn());
        presenter.setBorderAndTitleBar(hasBorder.isOn(), hasTitleBar.isOn());
        appWindow.setPresenter(presenter);
        appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
        appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);

        maximizeButton.add_onClick([presenter](auto&&...) {
            if (presenter.state() == OverlappedPresenterState::Maximized) {
                presenter.restore();
            } else {
                presenter.maximize();
            }
        });
        window.add_onSizeChanged([presenter, maximizeButton](auto&&...) {
            maximizeButton.content(presenter.state() == OverlappedPresenterState::Maximized ? u"Restore" : u"Maximize");
        });
        minimizeButton.add_onClick([presenter](auto&&...) { presenter.minimize(); });
        restoreButton.add_onClick([presenter](auto&&...) {
            presenter.minimize();
            auto timer = DispatcherQueue::getForCurrentThread().createTimer();
            timer.interval(std::chrono::seconds {3});
            timer.isRepeating(false);
            timer.add_onTick([presenter, timer](auto&&...) {
                presenter.restore();
                timer.stop();
            });
            timer.start();
        });
        closeButton.add_onClick([appWindow](auto&&...) { appWindow.destroy(); });

        gallery::trackWindow(window);
        window.activate();
    }
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"OverlappedPresenter is the default presenter for AppWindow, providing a standard resizable window with "
        u"system buttons. It is used for typical app windows and can be customized to control resizing and button "
        u"visibility.",
    },
    InfoBar {
        title = u"Warning",
        isClosable = false,
        isOpen = true,
        severity = InfoBarSeverity::Warning,
        content = RichTextBlock {
            Margin {0, -16, 16, 16},
            isTextSelectionEnabled = false,
            Paragraph {
                Run {u"For an "},
                Run {u"AppWindow", FontWeight {600}},
                Run {u" with "},
                Run {u"OverlappedPresenter", FontWeight {600}},
                Run {u", if the title bar ("},
                Run {u"HasTitleBar = true", FontWeight {600}},
                Run {u") is enabled, the window must have a border ("},
                Run {u"HasBorder = true", FontWeight {600}},
                Run {u"). Setting "},
                Run {u"HasBorder", FontWeight {600}},
                Run {u" to "},
                Run {u"false", FontWeight {600}},
                Run {u" while "},
                Run {u"HasTitleBar", FontWeight {600}},
                Run {u" is "},
                Run {u"true", FontWeight {600}},
                Run {u" will result in a fatal error."},
            },
        },
    },
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
};

auto options = StackPanel {
    spacing = 8,
    model->alwaysOnTop,
    model->maximizable,
    model->minimizable,
    model->resizable,
    model->hasBorder,
    model->hasTitleBar,
};