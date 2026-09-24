StackPanel {
    orientation.horizontal,
    spacing = 16.0,
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(24, 12, 4),
        TextBlock {
            u"0.3",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(255, 236, 200),
            GaussianBlurEffect {color = rgb(255, 122, 0), blurRadius = 24.0f, gamma = 0.3},
        },
    },
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(24, 12, 4),
        TextBlock {
            u"1.0",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(255, 236, 200),
            GaussianBlurEffect {color = rgb(255, 122, 0), blurRadius = 24.0f, gamma = 1.0},
        },
    },
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(24, 12, 4),
        TextBlock {
            u"3.0",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(255, 236, 200),
            GaussianBlurEffect {color = rgb(255, 122, 0), blurRadius = 24.0f, gamma = 3.0},
        },
    },
}
