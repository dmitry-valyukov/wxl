constexpr char16_t const* cacheDescription =
    u"Deletes all cached items when closing the browser. This includes cookies, images, and browsing history.";
auto example = StackPanel {
    spacing = 8.0,
    // Use FullDescription to connect visible descriptions to their controls
    StackPanel {
        spacing = 8.0,
        CheckBox {automationFullDescription = cacheDescription, content = u"Clear cache on exit"},
        TextBlock {automationAccessibilityView = AccessibilityView::Raw, foreground = brushes.Text.FillColor.Secondary, cacheDescription},
    },
    // Use HelpText and/or tooltips to explain nuances of controls
    Button {automationHelpText = u"Launch the cancellation wizard", content = u"Cancel RSS subscriptions",
            toolTip = u"Launch the cancellation wizard"},
};
