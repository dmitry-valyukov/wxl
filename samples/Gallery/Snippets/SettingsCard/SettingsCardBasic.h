auto example = StackPanel {
    spacing = 4.0,
    SettingsCard {
        header = u"App theme",
        description = u"Select which app theme to display",
        headerIcon = FontIcon {glyph = u""},
        ComboBox {
            ComboBoxItem {content = u"Light"},
            ComboBoxItem {content = u"Dark"},
            ComboBoxItem {content = u"Use system setting"},
            selectedIndex = 2,
        },
    },
    SettingsCard {
        header = u"Notifications",
        description = u"Show a banner when something happens",
        headerIcon = FontIcon {glyph = u""},
        ToggleSwitch {},
    },
    // The header is a name only: no description, no icon.
    SettingsCard {header = u"Language", Button {u"Change"}},
};
