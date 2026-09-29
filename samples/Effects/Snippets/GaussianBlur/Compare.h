StackPanel {
    orientation.horizontal,
    spacing = 16.0,
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(8, 11, 16),
        TextBlock {
            u"Halo",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(234, 247, 255),
            HaloEffect {color = rgb(43, 179, 243), blurRadius = 18.0f},
        },
    },
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(8, 11, 16),
        TextBlock {
            u"Blur",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(234, 247, 255),
            GaussianBlurEffect {color = rgb(43, 179, 243), blurRadius = 18.0f, gamma = 1.0},
        },
    },
    Border {
        CornerRadius {8},
        Padding {20, 12},
        background = rgb(8, 11, 16),
        TextBlock {
            u"γ 0.4",
            fontSize = 40,
            FontWeight {700},
            foreground = rgb(234, 247, 255),
            GaussianBlurEffect {color = rgb(43, 179, 243), blurRadius = 18.0f, gamma = 0.4},
        },
    },
}
