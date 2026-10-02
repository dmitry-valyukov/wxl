auto example = StackPanel {
    spacing = 8,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"The FullScreenPresenter makes an AppWindow cover the entire screen, removing the title bar and system UI to "
        u"create an immersive experience. To ensure usability, an exit mechanism, such as handling the Escape key or "
        u"close button, should be included, and fullscreen mode should be used in scenarios like media playback or "
        u"focused tasks.",
    },
    Button {
        content = u"Show window (Fullscreen mode)",
        onClick = [](Button const&) {
            Button closeButton {
                width = 200,
                hAlign.center,
                automationName = u"Close",
                content = StackPanel {
                    orientation.horizontal,
                    vAlign.center,
                    SymbolIcon {symbol = Symbol::Cancel, Margin {0, 0, 4, 0}, foreground = brushes.SystemFillColor.Critical},
                    TextBlock {foreground = brushes.SystemFillColor.Critical, u"Close"},
                },
            };
            Window window {
                systemBackdrop = MicaBackdrop {},
                content = StackPanel {
                    hAlign.center,
                    vAlign.center,
                    spacing = 8,
                    TextBlock {styles.TextBlock.Title, textAlignment = TextAlignment::Center, u"This window is running in Fullscreen mode"},
                    closeButton,
                },
            };
            auto appWindow = window.appWindow();
            appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
            appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);
            closeButton.add_onClick([appWindow](auto&&...) { appWindow.destroy(); });

            // Set the window to Full-Screen mode
            appWindow.setPresenter(AppWindowPresenterKind::FullScreen);

            gallery::trackWindow(window);
            window.activate();
        },
    },
};