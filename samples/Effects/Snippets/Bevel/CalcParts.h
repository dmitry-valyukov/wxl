// Цвета клавиши: верх и низ лица, надпись.
struct KeyColors {
    uint32_t top;
    uint32_t bottom;
    uint32_t ink;
};

constexpr KeyColors graphite {0xFF5B5F6B, 0xFF2B2D34, 0xFFF2F3F5};
constexpr KeyColors navy {0xFF4A5E92, 0xFF222E52, 0xFFE3EAFB};
constexpr KeyColors amber {0xFFD98A3A, 0xFF8E4A12, 0xFFFFF1DC};

// Лицо: градиент сверху вниз, ось чуть отклонена от вертикали.
LinearGradientBrush gradient(uint32_t begin, uint32_t end) {
    return LinearGradientBrush {
        startPoint = Point{0.4f, 0.0f},
        endPoint = Point{0.6f, 1.0f},
        GradientStop {ARGB{begin}, offset = 0.0},
        GradientStop {ARGB{end}, offset = 1.0},
    };
}

// Клавиша — два Border один в другом. Внешний даёт тёмную обводку и лицо,
// внутренний — лёгкий кант BevelEffect: блик слева сверху, тень справа снизу.
Border CalcButton(const char16_t* label, KeyColors colors, int r, int c) {
    return Border {
        row = r,
        column = c,
        CornerRadius {8},
        BorderThickness {1},
        borderBrush = ARGB{0xFF101218},
        background = gradient(colors.top, colors.bottom),

        Border {
            CornerRadius {7},
            BorderThickness {2},
            BevelEffect {
                {0x46FFFFFF, 0.0},
                {0x28FFFFFF, 0.4999},
                {0x30000000, 0.5001},
                {0x60000000, 1.0},
            },
            TextBlock {
                label,
                fontSize = 30,
                FontWeight {700},
                hAlign.center,
                vAlign.center,
                foreground = ARGB{colors.ink},
            },
        },
    };
}

// Корпус: сине-серое лицо и такой же лёгкий кант.
Border CalcBody(Grid const& keys) {
    return Border {
        hAlign.center,
        CornerRadius {16},
        BorderThickness {2},
        BevelEffect {
            {0x50FFFFFF, 0.0},
            {0x30FFFFFF, 0.4999},
            {0x40000000, 0.5001},
            {0x80000000, 1.0},
        },
        background = gradient(0xFF6D74A6, 0xFF3E4370),
        keys,
    };
}
