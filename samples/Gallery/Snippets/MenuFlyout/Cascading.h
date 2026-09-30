auto button = Button {
    content = u"File Options",
    flyout = MenuFlyout {
        MenuFlyoutItem {text = u"Open"},
        MenuFlyoutSubItem {
            text = u"Send to",
            MenuFlyoutItem {text = u"Bluetooth"},
            MenuFlyoutItem {text = u"Desktop (shortcut)"},
            MenuFlyoutSubItem {
                text = u"Compressed file",
                MenuFlyoutItem {text = u"Compress and email"},
                MenuFlyoutItem {text = u"Compress to .7z"},
                MenuFlyoutItem {text = u"Compress to .zip"},
            },
        },
    },
};