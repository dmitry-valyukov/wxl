StackPanel {
    orientation.horizontal,
    spacing = 16.0,
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(16, 20, 28),
        TextBlock {
            u"под",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(255, 255, 255),
            GaussianBlurEffect {color = rgb(255, 255, 255), blurRadius = 8.0f, opacity = 0.9},
        },
    },
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(16, 20, 28),
        TextBlock {
            u"над",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(255, 255, 255),
            GaussianBlurEffect {color = rgb(255, 255, 255), blurRadius = 8.0f, opacity = 0.9, zIndex = 1},
        },
    },
}
