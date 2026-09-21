// Тёмный корпус: на нём видно, где светлая половина кромки переходит в тёмную.
Border {
    CornerRadius {10},
    Padding {24, 20},
    background = ARGB{0xFF4A4F57},
    StackPanel {
        spacing = 16.0,

        // BevelEffect: перелом цвета точно в двух углах при любой ширине.
        Border {
            height = 64,
            CornerRadius {6},
            BorderThickness {4},
            background = ARGB{0xFFB4B4B4},
            BevelEffect {
                {0xFFFFFFFF, 0.0},
                {0xFFFFFFFF, 0.4999},
                {0xFF3C3C3C, 0.5001},
                {0xFF3C3C3C, 1.0},
            },
            TextBlock {u"BevelEffect", hAlign.center, vAlign.center},
        },

        // Тот же градиент руками, от угла к углу: на неквадратной рамке линии
        // равного смещения идут поперёк оси в пикселях, и перелом уезжает из углов.
        Border {
            height = 64,
            CornerRadius {6},
            BorderThickness {4},
            background = ARGB{0xFFB4B4B4},
            borderBrush = LinearGradientBrush {
                startPoint = Point{0, 0},
                endPoint = Point{1, 1},
                GradientStop {ARGB{0xFFFFFFFF}, offset = 0.0},
                GradientStop {ARGB{0xFFFFFFFF}, offset = 0.4999},
                GradientStop {ARGB{0xFF3C3C3C}, offset = 0.5001},
                GradientStop {ARGB{0xFF3C3C3C}, offset = 1.0},
            },
            TextBlock {u"LinearGradientBrush {0, 0} — {1, 1}", hAlign.center, vAlign.center},
        },
    },
}
