auto example = StackPanel {
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"A modal window is a separate window that blocks interaction with its owner window until it is closed, often "
        u"used for critical actions like confirmations, authentication, or settings. Unlike a ContentDialog, which is "
        u"a lightweight pop-up within the same window, a modal window is a fully independent window, making it "
        u"suitable for multi-window applications or scenarios requiring more flexibility in layout and behavior.",
    },
    Button {
        Margin {0, 8, 0, 0},
        content = u"Show modal window",
        onClick = [](Button const&) {
            Button okButton {width = 80, styles.Button.Accent, content = u"OK"};
            Button cancelButton {width = 80, content = u"Cancel"};
            Window window {
                systemBackdrop = MicaBackdrop {},
                content = StackPanel {
                    hAlign.center,
                    vAlign.center,
                    spacing = 8,
                    TextBlock {styles.TextBlock.Title, textAlignment = TextAlignment::Center, u"Modal Window"},
                    TextBlock {
                        styles.TextBlock.Body,
                        textAlignment = TextAlignment::Center,
                        textWrapping = TextWrapping::Wrap,
                        u"This is a modal window created using AppWindow with OverlappedPresenter.",
                    },
                    StackPanel {
                        orientation.horizontal,
                        hAlign.center,
                        spacing = 8,
                        okButton,
                        cancelButton,
                    },
                },
            };
            auto appWindow = window.appWindow();
            appWindow.resize({400, 300});
            appWindow.setIcon(u"Assets/Tiles/GalleryIcon.ico");
            appWindow.titleBar().preferredTheme(TitleBarTheme::UseDefaultAppMode);
            okButton.add_onClick([appWindow](auto&&...) { appWindow.destroy(); });
            cancelButton.add_onClick([appWindow](auto&&...) { appWindow.destroy(); });

            // The window is the dialog of the main one: its owner is set, and the owner is
            // disabled until the dialog closes.
            makeModalDialog(window, gallery::mainWindow());
            gallery::trackWindow(window);
            window.activate();
        },
    },
};