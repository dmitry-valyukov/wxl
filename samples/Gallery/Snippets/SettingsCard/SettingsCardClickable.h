struct Model {
    TextBlock result {Margin {0, 8, 0, 0}, u"Nothing clicked yet."};
};
auto const model = gallery::hold<Model>();

auto example = StackPanel {
    spacing = 4.0,
    SettingsCard {
        header = u"Open the project page",
        description = u"The whole card is a button: click it, or press Enter or Space on it",
        headerIcon = FontIcon {glyph = u""},
        isClickEnabled = true,
        onClick = [result = model->result](auto&&...) { result.text(u"The project page card was clicked."); },
    },
    SettingsCard {
        header = u"Copy the address",
        isClickEnabled = true,
        actionIcon = FontIcon {glyph = u""},
        TextBlock {u"https://github.com/microsoft/WinUI-Gallery", fontFamily = u"Consolas", foreground = brushes.Text.FillColor.Secondary},
        onClick = [result = model->result](auto&&...) { result.text(u"The address card was clicked: the action icon is a copy glyph."); },
    },
    model->result,
};
