#include "Calc.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {
    // Шаблон фона панели калькулятора: кисть строится при применении
    Template<RadialGradientBrush> backgroundTemplate(Color centerColor, Color edgeColor) {
        return {
            center = {0.33, 0.33},
            gradientOrigin = {0.33, 0.33},
            radiusX = 1.3,
            radiusY = 1.3,
            GradientStop {centerColor, offset = 0.0},
            GradientStop {edgeColor, offset = 1.0},
        };
    }

    // Тона «вдавленной» клавиши: ядро, кромка и её светлый край
    struct Dip {
        Color core, rim, edge;
    };

    // Полный набор тонов клавиши: покой, наведение, нажатие. Все производные
    // тона — сдвиг светлоты исходных в OKLCH, вычисленный компилятором, так
    // что набор целиком константа, а на старте только собираются кисти.
    struct KeyFace {
        Color ink, inkPressed;
        Dip rest, hover, pressed;
    };

    consteval Dip dip(Color core, Color rim) {
        return {core, rim, lightness(rim, 0.06)};
    }

    consteval KeyFace keyFace(Color ink, Color core, Color rim) {
        return {
            ink, lightness(ink, -0.15),
            dip(core, rim),
            dip(lightness(core, 0.06), lightness(rim, 0.06)),
            dip(lightness(core, -0.03), lightness(rim, -0.05)),
        };
    }

    // Эффект «вдавленной» в панель клавиши
    Template<RadialGradientBrush> dipTemplate(Dip const& tones) {
        return {
            center = {0.5, 0.5},
            gradientOrigin = {0.1, 0.1},
            radiusX = 1.85,
            radiusY = 1.15,
            GradientStop {tones.core, offset = 0.0},
            GradientStop {tones.rim, offset = 0.75},
            GradientStop {tones.edge, offset = 1.0},
        };
    }

    // Настройка стилей кнопки и её состояний (PointerOver, Pressed) через словарь тем
    auto keyFacePreset(KeyFace const& face) {
        return Preset{
            foreground = SolidColorBrush {face.ink},
            background = dipTemplate(face.rest),

            ThemeBrush {u"ButtonBackgroundPointerOver", dipTemplate(face.hover).build()},
            ThemeBrush {u"ButtonBackgroundPressed", dipTemplate(face.pressed).build()},
            ThemeBrush {u"ButtonForegroundPointerOver", SolidColorBrush {face.ink}},
            ThemeBrush {u"ButtonForegroundPressed", SolidColorBrush {face.inkPressed}},
            ThemeBrush {u"ButtonBorderBrushPointerOver", SolidColorBrush {colors.black}},
            ThemeBrush {u"ButtonBorderBrushPressed", SolidColorBrush {colors.black}},

            borderBrush = SolidColorBrush {colors.black},
            BorderThickness {1},
            CornerRadius {6},

            Padding {1},
            horizontalContentAlignment = HorizontalAlignment::Stretch,
            verticalContentAlignment = VerticalAlignment::Stretch,
        };
    }

    // Палитры трёх видов клавиш: чернила, ядро (с прозрачностью) и кромка
    constexpr KeyFace numericKey  = keyFace(rgb(237, 239, 242), rgba(38, 40, 43, 0.933), rgb(70, 74, 81));
    constexpr KeyFace actionKey   = keyFace(rgb(204, 217, 245), rgba(29, 38, 58, 0.933), rgb(50, 70, 119));
    constexpr KeyFace terminalKey = keyFace(rgb(255, 227, 180), rgba(74, 44, 23, 0.933), rgb(182, 100, 29));

    // Кант для псевдообъёма: блик слева сверху, тень справа снизу. Рисует его
    // композитор поверх собственной обводки элемента.
    constexpr Color rimLight = rgba(255, 255, 255, 0.36);
    constexpr Color rimShade = rgba(0, 0, 0, 0.55);
}

using calculator::Calc;

wxl::Teardown wxl_launched() {
    // В простейших сценариях объекты типа Calc или отдельные
    // observable<T> можно оформлять в виде глобальных переменых и
    // не обслуживать время жизни динамически созданных объектов-моделей.
    //
    // В этом примере сценарий простейший, но принцип показан общий.

    // Модель, ссылочная семантика, подсчёт ссылок
    auto const calcKeeper = Calc::create();

    // Голый указатель для дешевого захвата
    Calc * calc = calcKeeper.get();

    // Общий стиль для шрифтового оформления LCD-экрана калькулятора
    auto const lcdDisplayPreset = Preset{
        fontFamily = FontFamily{u"Assets/digitalism.ttf#Digitalism"},
        foreground = Template<SolidColorBrush>{rgb(44, 58, 28)},
        hAlign.stretch,
        textAlignment.right,
        FontWeight {600},
        HaloEffect {color = rgb(92, 138, 32), blurRadius = 14.0f},
    };

    // Клавиши: кант внутри чёрной обводки, свет чуть заходит на правую сторону.
    auto const rim = BevelEffect {rimLight, rimShade, Margin {1}, offset = 2};

    // Стили для различных типов клавиш
    auto const numericKeyStyle  = keyFacePreset(numericKey);
    auto const actionKeyStyle   = keyFacePreset(actionKey);
    auto const terminalKeyStyle = keyFacePreset(terminalKey);

    auto selectKeyPreset = [&](char16_t key) {
        return calculator::isNumeric(key)  ? numericKeyStyle
             : calculator::isTerminal(key) ? terminalKeyStyle
                                           : actionKeyStyle;
    };

    // Декларативное описание всего интерфейса в рамках единого C++ выражения
    auto mainWindow = Window {
        title = u"WXL Calculator",
        minSize = {320, 420},

        // Корпус — сама сетка клавиш: фон, кант и фокус надеты прямо на неё.
        content = Grid {
            requestedTheme = ElementTheme::Dark,
            BevelEffect {rimLight, rimShade, strokeThickness = 1},
            isTabStop = true,
            background = backgroundTemplate(rgba(35, 35, 100, 0.584), rgba(16, 16, 39, 0.584)),
            CornerRadius {6},
            Padding {16},
            rowSpacing = 8,
            columnSpacing = 8,
            rowDefinitions = u"auto,*,*,*,*,*",
            columnDefinitions = u"*,*,*,*",

            // Перехват текстового ввода символов
            onCharacterReceived = [calc](Object const&, CharacterReceivedRoutedEventArgs& args) {
                args.handled(calc->press(args.character()));
            },

            // Перехват системных горячих клавиш до стандартного роутинга диалогов WinUI 3
            onPreviewKeyDown = [calc](Object const&, KeyRoutedEventArgs& args) {
                switch (args.key()) {
                    case VirtualKey::Enter:  calc->press(u'='); break;
                    case VirtualKey::Escape: calc->press(u'C'); break;
                    default: return;
                }
                args.handled(true);
            },

            onLoaded = [](Grid const& keypad) { keypad.focus(FocusState::Programmatic); },

            // Табло: вдавлено тем же кантом с цветами в обратном порядке, поверх
            // тёмной обводки самого элемента.
            Border {
                row = 0,
                columnSpan = 4,
                background = backgroundTemplate(rgb(220, 232, 180), rgb(166, 178, 135)),
                borderBrush = rgba(51, 51, 51, 0.753),
                BorderThickness {3},
                BevelEffect {rimShade, rimLight},
                CornerRadius {10},
                Padding {14, 8, 14, 0},
                Margin {0, 4, 0, 12},

                Grid {
                    rowDefinitions = u"auto,*",

                    // Верхняя строка: текущее выражение (формула)
                    TextBlock {
                        lcdDisplayPreset,
                        row = 0,
                        fontSize = 28,
                        CharacterSpacing {100},
                        text = Bind{calc->expression},
                    },

                    // Нижняя строка: главное табло (число)
                    TextBlock {
                        lcdDisplayPreset,
                        row = 1,
                        Margin {0, 8, 0, -4},
                        fontSize = 64,
                        textWrapping.wrap,
                        CharacterSpacing {75},
                        text = Bind{calc->display},
                    },
                },
            },

            // Генерация сетки кнопок через compile-time фолд над строковым литералом
            [&](iterate<u"C÷×√"
                        u"789-"
                        u"456+"
                        u"123%"
                        u"±0.="> key) {
                return Button {
                    fontSize = 26,
                    FontWeight {600},
                    selectKeyPreset(key.value),
                    hAlign.stretch, vAlign.stretch,
                    row = key.index / 4 + 1,
                    column = key.index % 4,
                    onClick = [calc, symbol = key.value] { calc->press(symbol); },

                    rim,
                    content = TextBlock {
                        key.text(),
                        hAlign.center,
                        vAlign.center,
                        HaloEffect {color = rgb(43, 27, 0), blurRadius = 8.0f},
                    },
                };
            },
        },
    };

    auto appWindow = mainWindow.appWindow();
    appWindow.resize({420, 560});
    mainWindow.activate();

    // Продлеваем жизнь модели
    return [calcKeeper](TeardownReason) {};
}
