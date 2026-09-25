Border {
    CornerRadius {8},
    Padding {18, 12},
    background = rgb(18, 10, 6),
    borderBrush = rgb(52, 32, 15),
    BorderThickness {1},
    Grid {
        // Погашенные сегменты просвечивают под живыми — этим индикатор и
        // отличается от надписи.
        TextBlock {u"88:88", segment, foreground = rgba(255, 74, 0, 0.106)},

        // Два ореола на одном элементе: тугое ядро и широкий разлёт.
        TextBlock {
            u"12:34",
            segment,
            foreground = rgb(255, 217, 160),
            HaloEffect {color = rgb(255, 59, 0), blurRadius = 7.0f},
            HaloEffect {color = rgb(255, 106, 0), blurRadius = 28.0f},
        },
    },
}
