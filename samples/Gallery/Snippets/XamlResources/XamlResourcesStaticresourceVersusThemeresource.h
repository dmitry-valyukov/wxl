// StaticResource is a brush read once: resourceBrush. ThemeResource is a path of `brushes`: it is looked up again
// whenever the theme of the element changes.
auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 16}, u"Toggle the theme using the theme switch button in the top right corner."},
    Grid {
        background = resourceBrush(u"SolidBackgroundFillColorBaseBrush"),
        TextBlock {fontSize = 16, textWrapping = TextWrapping::Wrap, foreground = resourceBrush(u"TextFillColorPrimaryBrush"),
                   u"A brush read by resourceBrush is the one of the theme the application had when it started and does not update when "
                   u"the theme changes."},
    },
    Grid {
        background = brushes.SolidBackgroundFillColor.Base,
        TextBlock {fontSize = 16, textWrapping = TextWrapping::Wrap, foreground = brushes.Text.FillColor.Primary,
                   u"A path of brushes adapts automatically to the current theme. If the app switches from light to dark, the color "
                   u"changes."},
    },
};
