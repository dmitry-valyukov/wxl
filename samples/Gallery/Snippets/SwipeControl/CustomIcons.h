auto swipe = SwipeControl {
    width = 500,
    height = 68,
    Margin {12},
    borderBrush = brushes.Control.FillColor.Default,
    BorderThickness {1},
    leftItemsMode = SwipeMode::Reveal,
    leftItems[SwipeItem {
        background = themeBrushes.Button.Background,
        foreground = themeBrushes.AppBar.Item.Foreground,
        iconSource = BitmapIconSource {uriSource = u"Assets/SampleMedia/CoffeeCup.png"},
        text = u"Coffee",
    }],
    content = TextBlock {Margin {12}, hAlign.center, vAlign.center, u"Swipe Right"},
};