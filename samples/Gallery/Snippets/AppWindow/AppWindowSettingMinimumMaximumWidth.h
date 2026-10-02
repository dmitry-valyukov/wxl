struct Model {
    NumberBox minWidthBox {header = u"PreferredMinimumWidth", value = 400.0};
    NumberBox minHeightBox {header = u"PreferredMinimumHeight", value = 400.0};
    NumberBox maxWidthBox {header = u"PreferredMaximumWidth", value = 1000.0};
    NumberBox maxHeightBox {header = u"PreferredMaximumHeight", value = 1000.0};

    void open() {
        Button minimizeButton {width = 200, content = u"Minimize"};
        Button restoreButton {width = 200, content = u"Minimize and restore after 3 seconds"};
        Button closeButton {width = 200, content = u"Close"};
        Window window {
            systemBackdrop = MicaBackdrop {},
            content = StackPanel {hAlign.center, vAlign.center, spacing = 10, minimizeButton, restoreButton, closeButton},
        };
        auto appWindow = window.appWindow();
        appWindow.resize({800, 500});
        appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
        appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);

        auto presenter = OverlappedPresenter::create();
        presenter.preferredMinimumWidth(static_cast<int32_t>(minWidthBox.value()));
        presenter.preferredMinimumHeight(static_cast<int32_t>(minHeightBox.value()));
        presenter.preferredMaximumWidth(static_cast<int32_t>(maxWidthBox.value()));
        presenter.preferredMaximumHeight(static_cast<int32_t>(maxHeightBox.value()));
        presenter.isMaximizable(false);
        appWindow.setPresenter(presenter);

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
        u"The minimum and maximum width and height can be set on an AppWindow. When setting the maximum width or "
        u"height, it's recommended to disable the window maximization.",
    },
    Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }},
};

auto options = StackPanel {
    orientation.vertical,
    spacing = 8,
    model->minWidthBox,
    model->minHeightBox,
    model->maxWidthBox,
    model->maxHeightBox,
};