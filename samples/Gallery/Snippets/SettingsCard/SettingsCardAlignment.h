auto example = StackPanel {
    spacing = 4.0,
    SettingsCard {header = u"Right (the default)", description = u"The setting stands right of the text", Button {u"Setting"}},
    SettingsCard {
        header = u"Left",
        description = u"The setting stands under the text, at its left",
        contentAlignment = SettingsCardContentAlignment::Left,
        Button {u"Setting"},
    },
    SettingsCard {
        header = u"Vertical",
        description = u"The setting stands under the text and is as wide as the card",
        contentAlignment = SettingsCardContentAlignment::Vertical,
        Slider {minimum = 0, maximum = 100, value = 40},
    },
    SettingsCard {header = u"Disabled", description = u"A disabled card is dimmed", isEnabled = false, ToggleSwitch {}},
};
