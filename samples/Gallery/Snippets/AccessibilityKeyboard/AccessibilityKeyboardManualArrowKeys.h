// XYFocusKeyboardNavigation enables arrow keys between the buttons.
//
// Note that:
//
// - The buttons are still tabbable by default
// - Home/End and PgUp/PgDn do not work
// - Screen readers do not read PositionInSet/SizeOfSet ("1 of 3")
//
// All of that would require custom work.
auto example = StackPanel {
    spacing = 4.0,
    Border {
        Padding {8},
        automationName = u"Potatoes?",
        background = brushes.Card.BackgroundFillColor.Secondary,
        borderBrush = brushes.SurfaceStrokeColor.Default,
        BorderThickness {1},
        cornerRadius = CornerRadius {4},
        child = StackPanel {
            spacing = 8.0,
            TextBlock {styles.TextBlock.BodyStrong, u"Potatoes?"},
            StackPanel {
                orientation.horizontal,
                spacing = 4.0,
                xyFocusKeyboardNavigation = XYFocusKeyboardNavigationMode::Enabled,
                Button {content = u"Boil 'em"},
                Button {content = u"Mash 'em"},
                Button {content = u"Stick 'em in a stew"},
            },
        },
    },
    TextBlock {styles.TextBlock.Caption,
               u"Arrow keys navigate between each button, but tab still navigates between each button, and other buttons like Home/End do "
               u"not work"},
};
