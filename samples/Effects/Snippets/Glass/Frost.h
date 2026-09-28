Grid {
    width = 520,
    height = 300,
    Image {
        source = u"Assets/board.jpg",
        stretch = Stretch::UniformToFill,
    },
    Border {
        hAlign.center,
        vAlign.center,
        CornerRadius {12},
        Padding {28, 18},
        background = rgba(255, 255, 255, 0.15),
        GlassEffect {blurRadius = 24.0f},
        TextBlock {
            u"x² − 4x − 6 = 0",
            fontSize = 30,
            FontWeight {700},
            foreground = rgb(20, 24, 32),
        },
    },
}
