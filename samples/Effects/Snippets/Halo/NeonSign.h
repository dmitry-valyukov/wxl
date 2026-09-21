Border {
    CornerRadius {8},
    Padding {24, 22},
    background = rgb(11, 7, 20),
    StackPanel {
        orientation.horizontal,
        spacing = 28.0,
        hAlign.center,

        // Кольцо и бокал в нём: светятся обводка кольца и линии картинки.
        Grid {
            Ellipse {
                width = 86,
                height = 86,
                stroke = rgb(184, 251, 255),
                strokeThickness = 4.0,
                HaloEffect {color = rgb(0, 200, 255), blurRadius = 18.0f},
            },
            Image {
                source = u"Assets/Coctail.png",
                width = 50,
                height = 50,
                HaloEffect {color = rgb(0, 200, 255), blurRadius = 10.0f},
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
                stroke = rgb(255, 196, 246),
                strokeThickness = 4.0,
                HaloEffect {color = rgb(255, 43, 214), blurRadius = 18.0f},
            },
            TextBlock {
                u"Night Club",
                fontFamily = u"Assets/neonderthaw.ttf#NeonDerthaw",
                fontSize = 40,
                renderTransformOrigin = {0.5, 0.5},
                renderTransform = RotateTransform {angle = -8.0},
                hAlign.center,
                vAlign.center,
                foreground = rgb(255, 232, 251),
                HaloEffect {color = rgb(255, 43, 214), blurRadius = 14.0f},
            },
        },

        // Залитая лампочка: два ореола, ядро и разлёт.
        Ellipse {
            width = 28,
            height = 28,
            vAlign.center,
            fill = rgb(255, 246, 176),
            HaloEffect {color = rgb(255, 196, 0), blurRadius = 12.0f},
            HaloEffect {color = rgb(255, 122, 0), blurRadius = 36.0f},
        },
    },
}
