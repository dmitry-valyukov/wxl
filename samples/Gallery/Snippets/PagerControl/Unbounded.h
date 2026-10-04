StackPanel {
    spacing = 20.0,
    TextBlock {
        u"Set NumberOfPages to -1 when the final page is not known. "
        u"Unbounded pagers omit the total-page count and last-page navigation.",
        textWrapping = TextWrapping::Wrap,
    },
    StackPanel {
        spacing = 8.0,
        TextBlock {u"Number box", styles.TextBlock.BodyStrong},
        PagerControl {
            automationId = u"PagerControlUnboundedNumberBox",
            automationName = u"Unbounded number box page selector",
            displayMode = PagerControlDisplayMode::NumberBox,
            numberOfPages = -1,
            prefixText = u"Page",
            selectedPageIndex = 99,
        },
    },
    StackPanel {
        spacing = 8.0,
        TextBlock {u"Button panel", styles.TextBlock.BodyStrong},
        PagerControl {
            automationId = u"PagerControlUnboundedButtonPanel",
            automationName = u"Unbounded button panel page selector",
            displayMode = PagerControlDisplayMode::ButtonPanel,
            firstButtonVisibility = PagerControlButtonVisibility::Visible,
            nextButtonVisibility = PagerControlButtonVisibility::Visible,
            numberOfPages = -1,
            previousButtonVisibility = PagerControlButtonVisibility::Visible,
            selectedPageIndex = 99,
        },
    },
}