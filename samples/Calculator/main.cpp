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

    // Смещение цветовых каналов на заданную величину с зажатием в диапазоне 0..255
    constexpr Color tweakColor(Color color, int by) {
        auto const channel = [by](uint8_t value) {
            int const moved = value + by;
            return static_cast<uint8_t>(moved < 0 ? 0 : moved > 255 ? 255 : moved);
        };

        return {color.A, channel(color.R), channel(color.G), channel(color.B)};
    }

    // Эффект «вдавленной» в панель клавиши
    Template<RadialGradientBrush> dipTemplate(Color core, Color rim) {
        return {
            center = {0.5, 0.5},
            gradientOrigin = {0.1, 0.1},
            radiusX = 1.85,
            radiusY = 1.15,
            GradientStop {core, offset = 0.0},
            GradientStop {rim, offset = 0.75},
            GradientStop {tweakColor(rim, 18), offset = 1.0},
        };
    }

    // Настройка стилей кнопки и её состояний (PointerOver, Pressed) через словарь тем
    auto keyFacePreset(Color ink, Color core, Color rim) {
        return Preset{
            foreground = SolidColorBrush {ink},
            background = dipTemplate(core, rim),

            ThemeBrush {u"ButtonBackgroundPointerOver", dipTemplate(tweakColor(core, 20), tweakColor(rim, 20)).build()},
            ThemeBrush {u"ButtonBackgroundPressed", dipTemplate(tweakColor(core, -5), tweakColor(rim, -12)).build()},
            ThemeBrush {u"ButtonForegroundPointerOver", SolidColorBrush {ink}},
            ThemeBrush {u"ButtonForegroundPressed", SolidColorBrush {tweakColor(ink, -40)}},
            ThemeBrush {u"ButtonBorderBrushPointerOver", SolidColorBrush {rgb(0, 0, 0)}},
            ThemeBrush {u"ButtonBorderBrushPressed", SolidColorBrush {rgb(0, 0, 0)}},

            borderBrush = SolidColorBrush {rgb(0, 0, 0)},
            BorderThickness {1},
            CornerRadius {6},

            Padding {1},
            horizontalContentAlignment = HorizontalAlignment::Stretch,
            verticalContentAlignment = VerticalAlignment::Stretch,
        };
    }

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
    auto const numericKeyStyle  = keyFacePreset(rgb(237, 239, 242), RGBA{"#26282BEE"}, rgb(70, 74, 81));
    auto const actionKeyStyle   = keyFacePreset(rgb(204, 217, 245), RGBA{"#1D263AEE"}, rgb(50, 70, 119));
    auto const terminalKeyStyle = keyFacePreset(rgb(255, 227, 180), RGBA{"#4A2C17EE"}, rgb(182, 100, 29));

    auto selectKeyPreset = [&](char16_t key) {
        return calculator::isNumeric(key)  ? numericKeyStyle
             : calculator::isTerminal(key) ? terminalKeyStyle
                                           : actionKeyStyle;
    };

    // Декларативное описание всего интерфейса в рамках единого C++ выражения
    auto mainWindow = Window {
        title = u"WXL Calculator",
        minSize = {320, 420},

        content = Border {
            requestedTheme = ElementTheme::Dark,
            BevelEffect {rimLight, rimShade, strokeThickness = 1},
            isTabStop = true,
            background = backgroundTemplate(RGBA{"#23236495"}, RGBA{"#10102795"}),
            BorderThickness {1},
            CornerRadius {6},

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

            onLoaded = [](Border const& keypad) { keypad.focus(FocusState::Programmatic); },

            Grid {
                Padding {16},
                rowSpacing = 8,
                columnSpacing = 8,
                rowDefinitions = u"auto,*,*,*,*,*",
                columnDefinitions = u"*,*,*,*",

                // Экранная подложка
                Border {
                    row = 0,
                    columnSpan = 4,
                    background = backgroundTemplate(rgb(220, 232, 180), rgb(166, 178, 135)),
                    BorderThickness {2},
                    // Табло вдавлено: тот же кант с цветами в обратном порядке.
                    BevelEffect {rimShade, rimLight},
                    CornerRadius {10},
                    Padding {0},
                    Margin {0, 4, 0, 12},

                    Border {
                        BorderThickness {2},
                        borderBrush = SolidColorBrush {RGBA{"#333333C0"}},
                        CornerRadius {9},
                        Margin {-1},
                        Padding {14, 8, 14, 0},

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
        },
    };

    auto appWindow = mainWindow.appWindow();
    appWindow.resize({420, 560});
    mainWindow.activate();

    // Продлеваем жизнь модели
    return [calcKeeper](TeardownReason) {};
}
