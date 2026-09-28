Grid {
    width = 520,
    height = 300,
    Image {
        source = u"Assets/board.jpg",
        stretch = Stretch::UniformToFill,
    },
    StackPanel {
        orientation.horizontal,
        spacing = 16,
        hAlign.center,
        vAlign.center,
        Border {
            CornerRadius {12},
            Padding {18, 12},
            GlassEffect {rgba(255, 244, 214, 0.45), blurRadius = 12.0f},
            TextBlock {u"тёплое", fontSize = 22, FontWeight {700}, foreground = rgb(40, 30, 10)},
        },
        Border {
            CornerRadius {12},
            Padding {18, 12},
            GlassEffect {rgba(0, 0, 0, 0.4), blurRadius = 12.0f},
            TextBlock {u"тёмное", fontSize = 22, FontWeight {700}, foreground = rgb(255, 255, 255)},
        },
        Border {
            CornerRadius {12},
            Padding {18, 12},
            GlassEffect {blurRadius = 24.0f},
            TextBlock {u"без тинта", fontSize = 22, FontWeight {700}, foreground = rgb(255, 255, 255)},
        },
    },
}
