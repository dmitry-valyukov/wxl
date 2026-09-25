#include "Calc.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

    // Лампа сцены: сверху и немного слева — 105°, если считать от горизонтали
    // против часовой стрелки, — и поднята на 50° над поверхностью; тёплый
    // ключевой свет и холодноватый фоновый. В разметке заданы только альбедо
    // поверхностей, все тени и блики из них выводятся в compile-time по формулам
    // освещения.
    constexpr Light lamp {
        Direction::from_angles(105, 50),
        rgb(255, 246, 232),
        rgb(140, 146, 168)
    };

    // Тона освещённой панели: под лампой, на полпути и у края градиента.
    struct Glow {
        Color centre, mid, edge;
    };

    constexpr double glowRadius = 1.3;

    // Панель под лампой на высоте height (в радиусах градиента): фоновый
    // свет ровный, ключевой спадает от проекции лампы к краю.
    consteval Glow glow(Color albedo, double height) {
        return {
            relief_helper::under(albedo, lamp, 0.0, height),
            relief_helper::under(albedo, lamp, 0.5 * glowRadius, height),
            relief_helper::under(albedo, lamp, glowRadius, height),
        };
    }

    // Шаблон фона панели: кисть строится при применении
    Template<RadialGradientBrush> backgroundTemplate(Glow const& tones) {
        return {
            center = {0.33, 0.33},
            gradientOrigin = {0.33, 0.33},
            radiusX = glowRadius,
            radiusY = glowRadius,
            Template<GradientStop> {tones.centre, offset = 0.0},
            Template<GradientStop> {tones.mid, offset = 0.5},
            Template<GradientStop> {tones.edge, offset = 1.0},
        };
    }

    // Общий стиль для шрифтового оформления LCD-экрана калькулятора
    auto const lcdDisplayPreset = Preset {
        fontFamily = FontFamily{u"Assets/digitalism.ttf#Digitalism"},
        foreground = Template<SolidColorBrush>{rgb(44, 58, 28)},
        hAlign.stretch,
        textAlignment.right,
        FontWeight {600},
        HaloEffect {
            color = rgb(92, 138, 32),
            blurRadius = 14.0f
        },
    };

    // Альбедо корпуса и табло: тёмный полупрозрачный пластик и подсвеченный
    // изнутри экран, над которым лампа стоит вдвое выше и светит ровнее
    constexpr Glow bodyGlow = glow(rgba(39, 39, 89, 0.66), 1.0);
    constexpr Glow lcdGlow  = glow(rgb(208, 220, 170), 2.0);

    // Кант для псевдообъёма: блик слева сверху, тень справа снизу. Рисует его
    // композитор поверх собственной обводки элемента.
    constexpr Color rimLight = rgba(255, 255, 255, 0.46);
    constexpr Color rimShade = rgba(52, 52, 52, 0.55);

    // Три вида клавиш: чернила и материал (с прозрачностью), остальное —
    // свет: тень 0.7, вдавленный верх.
    Button3DEffect numericKey {
        foreground = rgb(237, 239, 242),
        background = rgba(68, 72, 79, 0.933),
        shadow = 0.4,
        emboss = -0.3
    };

    Button3DEffect actionKey {
        foreground = rgb(204, 217, 245),
        background = rgba(48, 68, 116, 0.933),
        shadow = 0.4,
        emboss = -0.3
    };

    Button3DEffect terminalKey {
        foreground = rgb(255, 227, 180),
        background = rgba(176, 98, 30, 0.933),
        shadow = 0.4,
        emboss = -0.3
    };

    // Раскладка клавиши — скругление, отступ
    // и растяжение содержимого. Вид клавиши даёт Button3DEffect.
    Preset keyLayoutPreset {
        CornerRadius {8},
        Padding {1},
        horizontalContentAlignment = hAlign.stretch,
        verticalContentAlignment = vAlign.stretch,
    };
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

    auto selectButtonEffect = [&](char16_t key) {
        return calculator::isNumeric(key)  ? numericKey
             : calculator::isTerminal(key) ? terminalKey
                                           : actionKey;
    };

    // Декларативное описание всего интерфейса в рамках единого C++ выражения
    auto mainWindow = Window {
        title = u"WXL Calculator",
        minSize = {320, 420},

        // Корпус — сама сетка клавиш: фон, кант и фокус надеты прямо на неё.
        content = Grid {
            requestedTheme = ElementTheme::Dark,
            BevelEffect {rimLight, rimShade, strokeThickness = 2},
            isTabStop = true,
            background = backgroundTemplate(bodyGlow),
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
                background = backgroundTemplate(lcdGlow),
                borderBrush = rgba(51, 51, 51, 0.753),
                BorderThickness {4},
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
                        text = BindOutput{calc->expression},
                    },

                    // Нижняя строка: главное табло (число)
                    TextBlock {
                        lcdDisplayPreset,
                        row = 1,
                        Margin {0, 8, 0, -4},
                        fontSize = 64,
                        textWrapping.wrap,
                        CharacterSpacing {75},
                        text = BindOutput{calc->display},
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
                    keyLayoutPreset,
                    selectButtonEffect(key.value),
                    hAlign.stretch, vAlign.stretch,
                    row = key.index / 4 + 1,
                    column = key.index % 4,
                    onClick = [calc, symbol = key.value] { calc->press(symbol); },

                    content = TextBlock {
                        key.text(),
                        fontSize = 28,
                        FontWeight {600},
                        hAlign.center,
                        vAlign.center,
                        GaussianBlurEffect {
                            color = rgb(0, 0, 0),
                            blurRadius = 3.0f,
                            gamma = 0.5
                        },
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
