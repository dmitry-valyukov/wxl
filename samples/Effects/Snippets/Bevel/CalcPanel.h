CalcBody(Grid {
    width = 280,
    Padding {18},
    rowSpacing = 10,
    columnSpacing = 10,
    rowDefinitions = u"auto,64,64",
    columnDefinitions = u"*,*",

    // Табло вдавлено: кант с теми же стопами в обратном порядке.
    Border {
        row = 0,
        columnSpan = 2,
        Margin {0, 0, 0, 6},
        Padding {14, 10},
        CornerRadius {8},
        BorderThickness {2},
        BevelEffect {
            {0xA5000000, 0.0},
            {0x75000000, 0.49},
            {0x55FFFFFF, 0.51},
            {0x65FFFFFF, 1.0},
        },
        background = gradient(0xFFDCE8B4, 0xFFA6B287),
        TextBlock {
            u"0",
            fontFamily = u"Assets/digitalism.ttf#Digitalism",
            fontSize = 48,
            FontWeight {600},
            textAlignment.right,
            textLineBounds.tight,
            vAlign.center,
            foreground = ARGB{0xFF2C3A1C},
            HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
        },
    },

    CalcButton(u"+", graphite, 1, 0),
    CalcButton(u"−", navy, 1, 1),
    CalcButton(u"×", graphite, 2, 0),
    CalcButton(u"=", amber, 2, 1),
})
