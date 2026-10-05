auto example = Button {
    content = u"Show centered sample window",
    onClick = [](Button const&) {
        Window window {
            systemBackdrop = MicaBackdrop {},
            content = StackPanel {
                hAlign.center,
                vAlign.center,
                spacing = 8,
                TextBlock {styles.TextBlock.Title, textAlignment = TextAlignment::Center, u"This is a centred sample window"},
            },
        };
        auto appWindow = window.appWindow();
        appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
        appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);

        // Centers the window on the screen based on the available display area.
        auto const area = DisplayArea::getFromWindowId(appWindow.id(), DisplayAreaFallback::Nearest).workArea();
        appWindow.move({(area.width - appWindow.size().width) / 2, (area.height - appWindow.size().height) / 2});

        gallery::trackWindow(window);
        window.activate();
    },
};