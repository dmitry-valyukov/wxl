Border {
    CornerRadius {8},
    Padding {18, 12},
    background = RadialGradientBrush {
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.3,
        radiusY = 1.3,
        GradientStop {rgb(220, 232, 180), offset = 0.0},
        GradientStop {rgb(166, 178, 135), offset = 1.0},
    },
    TextBlock {
        u"1234.56",
        fontFamily = u"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        FontWeight {600},
        CharacterSpacing {75},
        textAlignment.right,
        vAlign.center,
        foreground = rgb(44, 58, 28),
        HaloEffect {color = rgb(92, 138, 32), blurRadius = 14.0f},
    },
}
