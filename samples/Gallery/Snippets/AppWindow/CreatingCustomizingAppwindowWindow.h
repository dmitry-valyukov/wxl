struct Model {
    TextBox titleBox {placeholderText = u"Enter window title", hAlign.stretch};
    NumberBox widthBox = number(u"Width", 800, 200, 1000, 0);
    NumberBox heightBox = number(u"Height", 500, 200, 700, 1);
    NumberBox xBox = number(u"X", 50, 0, 800, 0);
    NumberBox yBox = number(u"Y", 50, 0, 300, 1);

    static NumberBox number(char16_t const* label, double initial, double least, double most, int boxColumn) {
        return NumberBox {
            header = label,
            column = boxColumn,
            Margin {0, 0, boxColumn == 0 ? 8 : 0, 0},
            largeChange = 100.0,
            minimum = least,
            maximum = most,
            smallChange = 10.0,
            spinButtonPlacementMode = NumberBoxSpinButtonPlacementMode::Inline,
            value = initial,
        };
    }

    void open() {
        // A window of three buttons: the first hides it, the second hides it and brings it
        // back after three seconds, the third closes it.
        Button hideButton {width = 200, toolTip = u"Hides the window from all UX representations in the system but keeps the object alive.", content = u"Hide"};
        Button showButton {width = 200, toolTip = u"Hides and then shows the window and activates it after 3 seconds.", content = u"Hide and show the window after 3 seconds"};
        Button closeButton {width = 200, Margin {0, 16, 0, 0}, toolTip = u"Closes the window and releases all resources.", content = u"Close"};
        Window window {
            content = StackPanel {hAlign.center, vAlign.center, spacing = 8, hideButton, showButton, closeButton},
        };
        auto appWindow = window.appWindow();
        hideButton.add_onClick([appWindow](auto&&...) { appWindow.hide(); });
        showButton.add_onClick([appWindow](auto&&...) {
            appWindow.hide();
            auto timer = DispatcherQueue::getForCurrentThread().createTimer();
            timer.interval(std::chrono::seconds {3});
            timer.isRepeating(false);
            timer.add_onTick([appWindow, timer](auto&&...) {
                appWindow.show();
                timer.stop();
            });
            timer.start();
        });
        closeButton.add_onClick([appWindow](auto&&...) { appWindow.destroy(); });

        // Set the window title
        appWindow.title(titleBox.text());
        // Set the window size (including borders)
        appWindow.resize({static_cast<int32_t>(widthBox.value()), static_cast<int32_t>(heightBox.value())});
        // Set the window position on screen
        appWindow.move({static_cast<int32_t>(xBox.value()), static_cast<int32_t>(yBox.value())});
        // Set the taskbar icon (displayed in the taskbar)
        appWindow.setTaskbarIcon(u"Assets/Tiles/GalleryIcon.ico");
        // Set the title bar icon (displayed in the window's title bar)
        appWindow.setTitleBarIcon(u"Assets/Tiles/GalleryIcon.ico");
        appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);

        gallery::trackWindow(window);
        window.activate();
    }
};
auto const model = gallery::hold<Model>();
model->titleBox.text(u"This is a title");

auto example = Button {content = u"Show window", onClick = [model](Button const&) { model->open(); }};

auto options = StackPanel {
    spacing = 8,
    TextBlock {FontWeight {600}, u"Window title"},
    model->titleBox,
    TextBlock {FontWeight {600}, u"Window size"},
    Grid {columnDefinitions = u"auto,auto", model->widthBox, model->heightBox},
    TextBlock {FontWeight {600}, u"Window postion"},
    Grid {columnDefinitions = u"auto,auto", model->xBox, model->yBox},
};