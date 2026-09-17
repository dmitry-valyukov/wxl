#include "Calc.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {
    // Базовая форма радиального градиента для эффекта свечения: константа,
    // выложенная компилятором, ничего не выполняется на старте
    constexpr Preset glowPreset{
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.3,
        radiusY = 1.3,
    };

    // Шаблон фона панели калькулятора: кисть строится при применении
    Template<RadialGradientBrush> createBackgroundTemplate(uint32_t centerColor, uint32_t edgeColor) {
        return {
            glowPreset,
            GradientStop {ARGB{centerColor}, offset = 0.0},
            GradientStop {ARGB{edgeColor}, offset = 1.0},
        };
    }

    // Смещение цветовых каналов на заданную величину с зажатием в диапазоне 0..255
    constexpr uint32_t tweakColor(uint32_t argb, int by) {
        auto const channel = [argb, by](int shift) {
            int const value = static_cast<int>((argb >> shift) & 0xFF) + by;
            return static_cast<uint32_t>(value < 0 ? 0 : value > 255 ? 255 : value) << shift;
        };

        return (argb & 0xFF000000) | channel(16) | channel(8) | channel(0);
    }

    // Эффект «вдавленной» в панель клавиши
    Template<RadialGradientBrush> createDipTemplate(uint32_t core, uint32_t rim) {
        return {
            center = {0.5, 0.5},
            gradientOrigin = {0.1, 0.1},
            radiusX = 1.85,
            radiusY = 1.15,
            GradientStop {ARGB{core}, offset = 0.0},
            GradientStop {ARGB{rim}, offset = 0.75},
            GradientStop {ARGB{tweakColor(rim, 18)}, offset = 1.0},
        };
    }

    // Настройка стилей кнопки и её состояний (PointerOver, Pressed) через словарь тем
    auto createKeyFacePreset(uint32_t ink, uint32_t core, uint32_t rim) {
        return Preset{
            foreground = SolidColorBrush {ARGB{ink}},
            background = createDipTemplate(core, rim),

            ThemeBrush {u"ButtonBackgroundPointerOver", createDipTemplate(tweakColor(core, 20), tweakColor(rim, 20)).build()},
            ThemeBrush {u"ButtonBackgroundPressed", createDipTemplate(tweakColor(core, -5), tweakColor(rim, -12)).build()},
            ThemeBrush {u"ButtonForegroundPointerOver", SolidColorBrush {ARGB{ink}}},
            ThemeBrush {u"ButtonForegroundPressed", SolidColorBrush {ARGB{tweakColor(ink, -40)}}},
            ThemeBrush {u"ButtonBorderBrushPointerOver", SolidColorBrush {ARGB{0xFF000000}}},
            ThemeBrush {u"ButtonBorderBrushPressed", SolidColorBrush {ARGB{0xFF000000}}},

            borderBrush = SolidColorBrush {ARGB{0xFF000000}},
            BorderThickness {1},
            CornerRadius {6},

            Padding {1},
            horizontalContentAlignment = HorizontalAlignment::Stretch,
            verticalContentAlignment = VerticalAlignment::Stretch,
        };
    }

    // Внутренний кант клавиши для формирования псевдообъема
    auto createKeyRimPreset() {
        return Preset{
            Margin {-1},
            BevelEffect {
                {0x65FFFFFF, 0.0},
                {0x55FFFFFF, 0.4999},
                {0x75000000, 0.5001},
                {0xA5000000, 1.0},
            },
            BorderThickness {2},
            CornerRadius {6},
        };
    }
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
        foreground = Template<SolidColorBrush>{ARGB{0xFF2C3A1C}},
        hAlign.stretch,
        textAlignment.right,
        FontWeight {600},
        HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
    };

    auto const rim = createKeyRimPreset();

    // Стили для различных типов клавиш
    auto const numericKeyStyle  = createKeyFacePreset(0xFFEDEFF2, 0xEE26282B, 0xFF464A51);
    auto const actionKeyStyle   = createKeyFacePreset(0xFFCCD9F5, 0xEE1D263A, 0xFF324677);
    auto const terminalKeyStyle = createKeyFacePreset(0xFFFFE3B4, 0xEE4A2C17, 0xFFB6641D);

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
            rim,
            Margin {0},
            isTabStop = true,
            background = createBackgroundTemplate(0x95232364, 0x95101027),
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
                    background = createBackgroundTemplate(0xFFDCE8B4, 0xFFA6B287),
                    BorderThickness {2},
                    BevelEffect {
                        {0xA5000000, 0.0},
                        {0x75000000, 0.49},
                        {0x55FFFFFF, 0.51},
                        {0x65FFFFFF, 1.0},
                    },
                    CornerRadius {10},
                    Padding {0},
                    Margin {0, 4, 0, 12},

                    Border {
                        BorderThickness {2},
                        borderBrush = SolidColorBrush {ARGB{0xC0333333}},
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

                        content = Border {
                            rim,
                            TextBlock {
                                key.text(),
                                hAlign.center,
                                vAlign.center,
                                HaloEffect {color = ARGB{0xFF2b1b00}, blurRadius = 8.0f},
                            },
                        },
                    };
                },
            },
        },
    };

    auto appWindow = mainWindow.appWindow();
    appWindow.resize({420, 560});
    appWindow.setIcon(u"Assets/Calc.ico");
    mainWindow.activate();

    // Продлеваем жизнь модели
    return [calcKeeper](TeardownReason) {};
}
