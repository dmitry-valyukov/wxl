auto example = StackPanel {
    orientation.vertical,
    MenuBar {
        MenuBarItem {title = u"File", accessKey = u"F",
                     MenuFlyoutItem {accessKey = u"N", text = u"New"}, MenuFlyoutItem {accessKey = u"O", text = u"Open..."},
                     MenuFlyoutItem {accessKey = u"S", text = u"Save"}, MenuFlyoutItem {accessKey = u"E", text = u"Exit"}},
        MenuBarItem {title = u"Edit", accessKey = u"E",
                     MenuFlyoutItem {accessKey = u"U", text = u"Undo"}, MenuFlyoutItem {accessKey = u"X", text = u"Cut"},
                     MenuFlyoutItem {accessKey = u"C", text = u"Copy"}, MenuFlyoutItem {accessKey = u"V", text = u"Paste"}},
        MenuBarItem {title = u"Help", accessKey = u"H", MenuFlyoutItem {accessKey = u"A", text = u"About"}},
    },
    TextBlock {styles.TextBlock.Caption, u"Press and release Alt to display Key Tips; use Alt+letter to move focus to items"},
};
