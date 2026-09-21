Border {
    CornerRadius {8},
    Padding {24, 22},
    background = ARGB{0xFF0B0714},
    StackPanel {
        orientation.horizontal,
        spacing = 28.0,
        hAlign.center,

        // Кольцо и бокал в нём: светятся обводка кольца и линии картинки.
        Grid {
            Ellipse {
                width = 86,
                height = 86,
                stroke = ARGB{0xFFB8FBFF},
                strokeThickness = 4.0,
                HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 18.0f},
            },
            Image {
                source = u"Assets/Coctail.png",
                width = 50,
                height = 50,
                HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 10.0f},
            },
        },

        // Рамка со скруглением и надпись в ней: ореол носят оба, каждый
        // свой.
        Grid {
            Rectangle {
                width = 230,
                height = 86,
                radiusX = 16,
                radiusY = 16,
                stroke = ARGB{0xFFFFC4F6},
                strokeThickness = 4.0,
                HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 18.0f},
            },
            TextBlock {
                u"Night Club",
                fontFamily = u"Assets/neonderthaw.ttf#NeonDerthaw",
                fontSize = 40,
                renderTransformOrigin = {0.5, 0.5},
                renderTransform = RotateTransform {angle = -8.0},
                hAlign.center,
                vAlign.center,
                foreground = ARGB{0xFFFFE8FB},
                HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 14.0f},
            },
        },

        // Залитая лампочка: два ореола, ядро и разлёт.
        Ellipse {
            width = 28,
            height = 28,
            vAlign.center,
            fill = ARGB{0xFFFFF6B0},
            HaloEffect {color = ARGB{0xFFFFC400}, blurRadius = 12.0f},
            HaloEffect {color = ARGB{0xFFFF7A00}, blurRadius = 36.0f},
        },
    },
}
