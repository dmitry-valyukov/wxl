Grid {
    housing,
    width = 280,
    Padding {18},
    rowSpacing = 10,
    columnSpacing = 10,
    rowDefinitions = u"auto,64,64",
    columnDefinitions = u"*,*",

    // Табло вдавлено: цвета канта в обратном порядке.
    Border {
        row = 0,
        columnSpan = 2,
        Margin {0, 0, 0, 6},
        Padding {14, 10},
        CornerRadius {8},
        BevelEffect {rgba(0, 0, 0, 0.55), rgba(255, 255, 255, 0.36)},
        background = gradient(rgb(220, 232, 180), rgb(166, 178, 135)),
        TextBlock {
            u"0",
            fontFamily = u"Assets/digitalism.ttf#Digitalism",
            fontSize = 48,
            FontWeight {600},
            textAlignment.right,
            textLineBounds.tight,
            vAlign.center,
            foreground = rgb(44, 58, 28),
            HaloEffect {color = rgb(92, 138, 32), blurRadius = 14.0f},
        },
    },

    Button {u"+", graphite, row = 1, column = 0},
    Button {u"−", navy, row = 1, column = 1},
    Button {u"×", graphite, row = 2, column = 0},
    Button {u"=", amber, row = 2, column = 1},
}
