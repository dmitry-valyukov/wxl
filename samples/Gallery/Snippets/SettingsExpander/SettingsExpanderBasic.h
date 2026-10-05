auto example = StackPanel {
    spacing = 4.0,
    SettingsExpander {
        header = u"Sound",
        description = u"Controls provide audible feedback",
        headerIcon = FontIcon {glyph = u""},
        // the setting of the head: the content
        ToggleSwitch {},
        // the cards under it
        items[SettingsCard {header = u"Enable Spatial Audio", description = u"Place sounds around the listener", ToggleSwitch {}},
              SettingsCard {header = u"Volume", contentAlignment = SettingsCardContentAlignment::Vertical, Slider {minimum = 0, maximum = 100, value = 60}}],
    },
    // Opened at once, the cards of the list can be clickable, and any other element can stand in the list.
    SettingsExpander {
        header = u"About",
        description = u"The cards of the list are dressed flat",
        headerIcon = FontIcon {glyph = u""},
        isExpanded = true,
        items[SettingsCard {header = u"Version", TextBlock {u"1.0.0", foreground = brushes.Text.FillColor.Secondary}},
              SettingsCard {header = u"Licence", isClickEnabled = true, TextBlock {u"MIT", foreground = brushes.Text.FillColor.Secondary}}],
    },
};
