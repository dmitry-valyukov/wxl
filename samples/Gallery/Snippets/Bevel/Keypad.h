// Лицо корпуса и табло: градиент сверху вниз, ось чуть отклонена от
// вертикали. Шаблон, а не кисть, и стопы в нём тоже шаблоны: всё строится,
// когда шаблон применяют, поэтому им можно заполнять константы уровня файла.
auto const gradient = [](Color begin, Color end) -> Template<LinearGradientBrush> {
    return {
        startPoint = Point{0.4f, 0.0f},
        endPoint = Point{0.6f, 1.0f},
        Template<GradientStop> {begin, offset = 0.0},
        Template<GradientStop> {end, offset = 1.0},
    };
};

// Раскладка клавиши: скругление, отступ под кант и растяжение содержимого.
// Вид даёт Button3DEffect ниже.
auto const keyLayout = Preset {
    hAlign.stretch,
    vAlign.stretch,
    fontSize = 30,
    FontWeight {700},
    CornerRadius {8},
    Padding {1},
    horizontalContentAlignment = hAlign.stretch,
    verticalContentAlignment = vAlign.stretch,
};

// Три вида клавиш, как в калькуляторе: чернила и материал, остальное — свет.
// Только верх выпуклый (emboss > 0), а не вдавленный. Кант клавиши — тот же
// BevelEffect, его присоединяет сам Button3DEffect.
Button3DEffect const graphite {
    foreground = rgb(242, 243, 245),
    background = rgb(68, 72, 79),
    shadow = 0.4,
    emboss = 0.3
};
Button3DEffect const navy {
    foreground = rgb(227, 234, 251),
    background = rgb(48, 68, 116),
    shadow = 0.4,
    emboss = 0.3
};
Button3DEffect const amber {
    foreground = rgb(255, 241, 220),
    background = rgb(176, 98, 30),
    shadow = 0.4,
    emboss = 0.3
};

// Корпус: сине-серое лицо и лёгкий кант, надетые прямо на Grid клавиш.
auto const housing = Preset {
    hAlign.center,
    CornerRadius {16},
    background = gradient(rgb(109, 116, 166), rgb(62, 67, 112)),
    BevelEffect {rgba(255, 255, 255, 0.3), rgba(0, 0, 0, 0.4), offset = 4},
};

auto example =
Grid {
    housing,
    Padding {18},
    columnSpacing = 10,
    rowDefinitions = u"64",
    columnDefinitions = u"64,64,148,64",
    Button {u"7", keyLayout, graphite, row = 0, column = 0},
    Button {u"8", keyLayout, graphite, row = 0, column = 1},
    Button {u"0", keyLayout, graphite, row = 0, column = 2},
    Button {u"=", keyLayout, amber, row = 0, column = 3},
};
