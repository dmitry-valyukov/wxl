auto swipe = SwipeControl {
    width = 500,
    height = 68,
    Margin {12},
    borderBrush = brushes.Control.FillColor.Default,
    BorderThickness {1},
    rightItemsMode = SwipeMode::Execute,
    rightItems[SwipeItem {
        background = LinearGradientBrush {
            startPoint = Offset {0, 0.5f},
            endPoint = Offset {1, 0.5f},
            gradientStops[
                GradientStop {offset = 0.0, color = rgb(0x89, 0x90, 0xf9)},
                GradientStop {offset = 0.5, color = rgb(0x5b, 0x66, 0xfb)},
                GradientStop {offset = 1.0, color = rgb(0x5c, 0x1d, 0xf4)}
            ],
        },
        behaviorOnInvoked = SwipeBehaviorOnInvoked::Close,
        iconSource = FontIconSource {glyph = u"\uE72E"},
        text = u"Lock",
    }],
    content = TextBlock {Margin {12}, hAlign.center, vAlign.center, u"Swipe Left"},
};