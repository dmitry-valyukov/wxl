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
            displayMode = PagerControlDisplayMode::ButtonPanel,
            firstButtonVisibility = PagerControlButtonVisibility::Visible,
            nextButtonVisibility = PagerControlButtonVisibility::Visible,
            numberOfPages = -1,
            previousButtonVisibility = PagerControlButtonVisibility::Visible,
            selectedPageIndex = 99,
        },
    },
}