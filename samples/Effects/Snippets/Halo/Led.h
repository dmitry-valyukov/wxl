Border {
    CornerRadius {8},
    Padding {18, 12},
    background = ARGB{0xFF120A06},
    borderBrush = ARGB{0xFF34200F},
    BorderThickness {1},
    Grid {
        // Погашенные сегменты просвечивают под живыми — этим индикатор и
        // отличается от надписи.
        TextBlock {u"88:88", segment, foreground = ARGB{0x1BFF4A00}},

        // Два ореола на одном элементе: тугое ядро и широкий разлёт.
        TextBlock {
            u"12:34",
            segment,
            foreground = ARGB{0xFFFFD9A0},
            HaloEffect {color = ARGB{0xFFFF3B00}, blurRadius = 7.0f},
            HaloEffect {color = ARGB{0xFFFF6A00}, blurRadius = 28.0f},
        },
    },
}
