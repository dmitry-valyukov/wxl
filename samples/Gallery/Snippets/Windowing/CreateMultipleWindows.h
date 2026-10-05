auto example = StackPanel {
    spacing = 12,
    TextBlock {
        textWrapping = TextWrapping::Wrap,
        u"This stable example uses AppWindow.ResizeClient to request a 500 by 500 physical-pixel client area.",
    },
    Button {
        automationId = u"WindowingCreateWindow",
        automationName = u"Create a top-level window",
        content = u"Create new Window",
        onClick = [](Button const& self) {
            Window childWindow {
                extendsContentIntoTitleBar = true,
                systemBackdrop = MicaBackdrop {},
                content = TextBlock {
                    u"New child window!",
                    hAlign.center,
                    vAlign.center,
                    requestedTheme = self.actualTheme(),
                },
            };

            // Gallery tracks child windows so they close when the app closes.
            gallery::trackWindow(childWindow);
            // ResizeClient takes physical pixels, independent of display scaling.
            childWindow.appWindow().resizeClient({500, 500});
            childWindow.activate();
        },
    },
};