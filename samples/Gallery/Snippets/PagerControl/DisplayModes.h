StackPanel {
    spacing = 20.0,
    StackPanel {
        spacing = 8.0,
        TextBlock {u"Number box", styles.TextBlock.BodyStrong},
        PagerControl {
            automationId = u"PagerControlNumberBox",
            automationName = u"Number box page selector",
            displayMode = PagerControlDisplayMode::NumberBox,
            numberOfPages = 50,
            prefixText = u"Page",
            selectedPageIndex = 9,
            suffixText = u"of",
        },
    },
    StackPanel {
        spacing = 8.0,
        TextBlock {u"Button panel", styles.TextBlock.BodyStrong},
        PagerControl {
            automationId = u"PagerControlButtonPanel",
            automationName = u"Button panel page selector",
            displayMode = PagerControlDisplayMode::ButtonPanel,
            firstButtonVisibility = PagerControlButtonVisibility::Visible,
            lastButtonVisibility = PagerControlButtonVisibility::Visible,
            nextButtonVisibility = PagerControlButtonVisibility::Visible,
            numberOfPages = 20,
            previousButtonVisibility = PagerControlButtonVisibility::Visible,
            selectedPageIndex = 4,
        },
    },
}