StackPanel {
    orientation.horizontal,
    spacing = 64.0,
    hAlign.center,
    Margin {0, 24},

    // С эффектом: растёт под указателем до 1.2.
    Button {
        u"Наведи на меня",
        width = 170,
        height = 50,
        MagnifyEffect {1.2},
    },

    // Эталон с той же надписью: увеличен на те же 1.2 всегда. XAML перерисовывает
    // содержимое под окончательный масштаб преобразования, так что это
    // то, как кнопка в 1.2 раза выглядит в идеале.
    Button {
        u"Наведи на меня",
        width = 170,
        height = 50,
        renderTransformOrigin = {0.5, 0.5},
        renderTransform = ScaleTransform {scaleX = 1.2, scaleY = 1.2},
    },
}
