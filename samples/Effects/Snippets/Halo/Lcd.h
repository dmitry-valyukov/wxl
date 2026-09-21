Border {
    CornerRadius {8},
    Padding {18, 12},
    background = RadialGradientBrush {
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.3,
        radiusY = 1.3,
        GradientStop {ARGB{0xFFDCE8B4}, offset = 0.0},
        GradientStop {ARGB{0xFFA6B287}, offset = 1.0},
    },
    TextBlock {
        u"1234.56",
        fontFamily = u"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        FontWeight {600},
        CharacterSpacing {75},
        textAlignment.right,
        vAlign.center,
        foreground = ARGB{0xFF2C3A1C},
        HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
    },
}
