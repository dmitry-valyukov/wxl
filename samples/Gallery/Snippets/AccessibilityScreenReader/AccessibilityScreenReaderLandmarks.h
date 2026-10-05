auto example = StackPanel {
    TextBlock {Margin {0, 0, 0, 10}, hAlign.left, styles.TextBlock.Caption, textWrapping = TextWrapping::Wrap,
               u"The sample below showcases landmarks. To navigate landmarks in Narrator, press D or Shift+D while in Scan mode."},
    Grid {
        columnDefinitions = u"200,*,200",
        // The navigation pane for our app
        StackPanel {
            Padding {6},
            automationLandmarkType = AutomationLandmarkType::Navigation,
            background = brushes.Card.StrokeColorDefault,
            cornerRadius = CornerRadius {4},
            spacing = 8.0,
            AutoSuggestBox {automationLandmarkType = AutomationLandmarkType::Search, placeholderText = u"Search"},
            Button {content = u"Open settings"},
        },
        // The main content of our app
        StackPanel {
            column = 1,
            Padding {6},
            automationLandmarkType = AutomationLandmarkType::Main,
            TextBlock {textWrapping = TextWrapping::WrapWholeWords,
                       u"Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod tempor incididunt ut labore et dolore magna "
                       u"aliqua. Ut enim ad minim veniam, quis nostrud exercitation ullamco laboris nisi ut aliquip ex ea commodo consequat. "
                       u"Duis aute irure dolor in reprehenderit in voluptate velit esse cillum dolore eu fugiat nulla pariatur. Excepteur sint "
                       u"occaecat cupidatat non proident, sunt in culpa qui officia deserunt mollit anim id est laborum"},
        },
        // A custom sidebar with a custom landmark name
        StackPanel {
            column = 2,
            Padding {6},
            automationLandmarkType = AutomationLandmarkType::Custom,
            automationLocalizedLandmarkType = u"Current viewers",
            background = brushes.Card.StrokeColorDefault,
            cornerRadius = CornerRadius {4},
            spacing = 8.0,
            TextBlock {automationHeadingLevel = AutomationHeadingLevel::Level1, styles.TextBlock.BodyStrong, u"Current viewers"},
            TextBlock {fontStyle = FontStyle::Italic, styles.TextBlock.Body, u"(No other users viewing)"},
        },
    },
};
