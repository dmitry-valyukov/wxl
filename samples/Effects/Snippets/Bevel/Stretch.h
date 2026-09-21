// Тёмный корпус: на нём видно, где светлая половина кромки переходит в тёмную.
Border {
    CornerRadius {10},
    Padding {24, 20},
    background = rgb(74, 79, 87),
    StackPanel {
        spacing = 16.0,

        // BevelEffect: резкий перелом точно в двух углах при любой ширине.
        Border {
            height = 64,
            CornerRadius {6},
            BorderThickness {4},
            background = rgb(180, 180, 180),
            BevelEffect {rgb(255, 255, 255), rgb(60, 60, 60), strokeThickness = 4, blurRadius = 0},
            TextBlock {u"BevelEffect", hAlign.center, vAlign.center},
        },

        // Тот же градиент руками, от угла к углу: на неквадратной рамке линии
        // равного смещения идут поперёк оси в пикселях, и перелом уезжает из углов.
        Border {
            height = 64,
            CornerRadius {6},
            BorderThickness {4},
            background = rgb(180, 180, 180),
            borderBrush = LinearGradientBrush {
                startPoint = Point{0, 0},
                endPoint = Point{1, 1},
                GradientStop {rgb(255, 255, 255), offset = 0.0},
                GradientStop {rgb(255, 255, 255), offset = 0.4999},
                GradientStop {rgb(60, 60, 60), offset = 0.5001},
                GradientStop {rgb(60, 60, 60), offset = 1.0},
            },
            TextBlock {u"LinearGradientBrush {0, 0} — {1, 1}", hAlign.center, vAlign.center},
        },
    },
}
