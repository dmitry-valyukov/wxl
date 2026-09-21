// Лицо: градиент сверху вниз, ось чуть отклонена от вертикали. Шаблон, а не
// кисть, и стопы в нём тоже шаблоны: всё строится, когда шаблон применяют,
// поэтому им можно заполнять константы уровня файла.
Template<LinearGradientBrush> gradient(Color begin, Color end) {
    return {
        startPoint = Point{0.4f, 0.0f},
        endPoint = Point{0.6f, 1.0f},
        Template<GradientStop> {begin, offset = 0.0},
        Template<GradientStop> {end, offset = 1.0},
    };
}

// Лёгкий кант: блик слева сверху, тень справа снизу. Один на все клавиши.
BevelEffect const rim {
    rgba(255, 255, 255, 0.25),
    rgba(171, 112, 112, 0.3),
    Margin {1},
    offset = 2
};

// Вид клавиши: тёмная обводка, лицо градиентом от верхнего цвета к нижнему,
// надпись своим цветом и кант поверх обводки. Пресет, а не кнопка: кнопки
// пишутся в разметке, а вид на них надевается.
auto keyLook(Color top, Color bottom, Color ink) {
    return Preset {
        hAlign.stretch,
        vAlign.stretch,
        fontSize = 30,
        FontWeight {700},
        CornerRadius {8},
        BorderThickness {1},
        borderBrush = rgb(16, 18, 24),
        background = gradient(top, bottom),
        foreground = ink,
        rim,
    };
}

auto const graphite = keyLook(rgb(91, 95, 107), rgb(43, 45, 52), rgb(242, 243, 245));
auto const navy = keyLook(rgb(74, 94, 146), rgb(34, 46, 82), rgb(227, 234, 251));
auto const amber = keyLook(rgb(217, 138, 58), rgb(142, 74, 18), rgb(255, 241, 220));

// Корпус: сине-серое лицо и такой же лёгкий кант, надетые прямо на Grid клавиш.
auto const housing = Preset {
    hAlign.center,
    CornerRadius {16},
    background = gradient(rgb(109, 116, 166), rgb(62, 67, 112)),
    BevelEffect {rgba(255, 255, 255, 0.3), rgba(0, 0, 0, 0.4), offset = 4},
};
