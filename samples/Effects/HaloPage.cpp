// Страница эффекта Halo: описание, примеры и их исходники. Раскладка страницы
// общая у всех эффектов (Showcase.cpp); здесь только то, что своё у Halo.
//
// Код примера лежит литералом непосредственно над самим примером: правка
// одного видна рядом с другим.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// ---- Описание -------------------------------------------------------------

constexpr const wchar_t* description = LR"HTML(
<h2>Halo Effect — свечение вокруг глифов</h2>
<p>Ореол — это <b>тень с нулевым смещением</b>, вырезанная по альфе
разложенных глифов. Своего цвета, радиуса и нулевого смещения у
<code>ThemeShadow</code> фреймворка нет — это тень высоты, а не света, —
поэтому ореол делается композицией, а не деревом XAML. И это единственное в
окне, чего декларативное дерево сказать не может.</p>
<p>Пишется он там, где ему место: в скобках самого элемента.</p>
<pre>TextBlock {
    u"0",
    fontSize = 48,
    HaloEffect { color = ARGB{0xFF5C8A20}, blurRadius = 14.0f },
}</pre>
<p><b>Композитора в этот момент взять неоткуда:</b> элемент ещё строится и
никакому окну не принадлежит. Эффект вместо этого <i>ждёт</i> — отсоединённая
корутина ждёт кадра (<code>CompositionTarget.Rendering</code>), пока XAML не
сделает элементу собственный дочерний визуал, и берёт композитора уже у него.
Кадр, а не событие элемента: о постройке своих композиционных детей XAML
ничего не объявляет, а на открывшемся окне, которого никто не трогал,
<code>LayoutUpdated</code> больше не приходит.</p>
<p>Дождавшись, ореол кладётся <code>insertAtBottom</code> — под глифы, —
и переставлять ничего не нужно.</p>
<p>Своего словаря у эффекта нет: в его скобках пишутся теги свойств самой
тени — <code>color</code>, <code>blurRadius</code>, <code>opacity</code>,
<code>offset</code>, — а хранится это как <code>Preset&lt;DropShadow&gt;</code>
и надевается на тень, когда та появится. Носить эффект может что угодно,
умеющее отдать альфу своих глифов; ограничение так и записано:
<code>requires { element.getAlphaMask(); }</code>.</p>
<p>Ореолы <b>складываются</b>: два на одном элементе дают тугое ядро и
широкий разлёт — так сделан светодиодный индикатор ниже.</p>
)HTML";

// ---- Примеры --------------------------------------------------------------

// Табло калькулятора: тёмно-зелёное свечение на оливковой подложке LCD.
constexpr const wchar_t* lcdCode = LR"CODE(
Border {
    CornerRadius {8},
    Padding {18, 12},
    background = RadialGradientBrush {
        center = {0.33, 0.33},
        gradientOrigin = {0.33, 0.33},
        radiusX = 1.3,
        radiusY = 1.3,
        GradientStop {ARGB{0xFFDCE8B4}, offset = 0.0},
        GradientStop {ARGB{0xFFA6B287}, offset = 1.0},
    },
    TextBlock {
        u"1234.56",
        fontFamily = u"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        FontWeight {600},
        CharacterSpacing {75},
        textAlignment.right,
        vAlign.center,
        foreground = ARGB{0xFF2C3A1C},
        HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
    },
}
)CODE";

FrameworkElement lcd() {
    return Border {
        CornerRadius {8},
        Padding {18, 12},
        background = RadialGradientBrush {
            center = {0.33, 0.33},
            gradientOrigin = {0.33, 0.33},
            radiusX = 1.3,
            radiusY = 1.3,
            GradientStop {ARGB{0xFFDCE8B4}, offset = 0.0},
            GradientStop {ARGB{0xFFA6B287}, offset = 1.0},
        },
        TextBlock {
            u"1234.56",
            fontFamily = u"Assets/digitalism.ttf#Digitalism",
            fontSize = 44,
            FontWeight {600},
            CharacterSpacing {75},
            textAlignment.right,
            vAlign.center,
            foreground = ARGB{0xFF2C3A1C},
            HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
        },
    };
}

// Светодиодный семисегментник: горячее ядро и красно-оранжевый разлёт.
constexpr const wchar_t* ledCode = LR"CODE(
// Общий вид разряда: и погашенного, и горящего.
auto const segment = Preset {
    fontFamily = u"Assets/digitalism.ttf#Digitalism",
    fontSize = 44,
    CharacterSpacing {75},
    textAlignment.right,
    vAlign.center,
};

Border {
    CornerRadius {8},
    Padding {18, 12},
    background = ARGB{0xFF120A06},
    borderBrush = ARGB{0xFF34200F},
    BorderThickness {1},
    Grid {
        // Погашенные сегменты просвечивают под живыми — этим индикатор и
        // отличается от надписи.
        TextBlock {u"88:88", segment, foreground = ARGB{0x14FF4A00}},

        // Два ореола на одном элементе: тугое ядро и широкий разлёт.
        TextBlock {
            u"12:34",
            segment,
            foreground = ARGB{0xFFFFD9A0},
            HaloEffect {color = ARGB{0xFFFF3B00}, blurRadius = 7.0f},
            HaloEffect {color = ARGB{0xFFFF6A00}, blurRadius = 28.0f},
        },
    },
}
)CODE";

FrameworkElement led() {
    auto const segment = Preset {
        fontFamily = u"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        CharacterSpacing {75},
        textAlignment.right,
        vAlign.center,
    };

    return Border {
        CornerRadius {8},
        Padding {18, 12},
        background = ARGB{0xFF120A06},
        borderBrush = ARGB{0xFF34200F},
        BorderThickness {1},
        Grid {
            // Погашенные сегменты просвечивают под живыми — этим индикатор и
            // отличается от надписи.
            TextBlock {u"88:88", segment, foreground = ARGB{0x14FF4A00}},

            // Два ореола на одном элементе: тугое ядро и широкий разлёт.
            TextBlock {
                u"12:34",
                segment,
                foreground = ARGB{0xFFFFD9A0},
                HaloEffect {color = ARGB{0xFFFF3B00}, blurRadius = 7.0f},
                HaloEffect {color = ARGB{0xFFFF6A00}, blurRadius = 28.0f},
            },
        },
    };
}

// Просто светящаяся надпись: обычная гарнитура, один ореол.
constexpr const wchar_t* glowCode = LR"CODE(
Border {
    CornerRadius {8},
    Padding {24, 20},
    background = ARGB{0xFF080B10},
    TextBlock {
        u"wxl",
        fontSize = 56,
        FontWeight {700},
        CharacterSpacing {80},
        hAlign.center,
        foreground = ARGB{0xFFEAF7FF},
        HaloEffect {color = ARGB{0xFF2BB3F3}, blurRadius = 24.0f},
    },
}
)CODE";

FrameworkElement glow() {
    return Border {
        CornerRadius {8},
        Padding {24, 20},
        background = ARGB{0xFF080B10},
        TextBlock {
            u"wxl",
            fontSize = 56,
            FontWeight {700},
            CharacterSpacing {80},
            hAlign.center,
            foreground = ARGB{0xFFEAF7FF},
            HaloEffect {color = ARGB{0xFF2BB3F3}, blurRadius = 24.0f},
        },
    };
}

// Неоновая вывеска из фигур: ореол носит и фигура, не только текст.
constexpr const wchar_t* shapesCode = LR"CODE(
Border {
    CornerRadius {8},
    Padding {24, 22},
    background = ARGB{0xFF0B0714},
    StackPanel {
        orientation.horizontal,
        spacing = 28.0,
        hAlign.center,

        // Кольцо и бокал в нём: светятся обводка кольца и линии картинки.
        Grid {
            Ellipse {
                width = 86,
                height = 86,
                stroke = ARGB{0xFFB8FBFF},
                strokeThickness = 4.0,
                HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 18.0f},
            },
            Image {
                source = u"Assets/Coctail.png",
                width = 50,
                height = 50,
                HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 10.0f},
            },
        },

        // Рамка со скруглением и надпись в ней: ореол носят оба, каждый
        // свой.
        Grid {
            Rectangle {
                width = 230,
                height = 86,
                radiusX = 16,
                radiusY = 16,
                stroke = ARGB{0xFFFFC4F6},
                strokeThickness = 4.0,
                HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 18.0f},
            },
            TextBlock {
                u"Night Club",
                fontFamily = u"Assets/neonderthaw.ttf#NeonDerthaw",
                fontSize = 40,
                renderTransformOrigin = {0.5, 0.5},
                renderTransform = RotateTransform {angle = -8.0},
                hAlign.center,
                vAlign.center,
                foreground = ARGB{0xFFFFE8FB},
                HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 14.0f},
            },
        },

        // Залитая лампочка: два ореола, ядро и разлёт.
        Ellipse {
            width = 28,
            height = 28,
            vAlign.center,
            fill = ARGB{0xFFFFF6B0},
            HaloEffect {color = ARGB{0xFFFFC400}, blurRadius = 12.0f},
            HaloEffect {color = ARGB{0xFFFF7A00}, blurRadius = 36.0f},
        },
    },
}
)CODE";

FrameworkElement shapes() {
    return Border {
        CornerRadius {8},
        Padding {24, 22},
        background = ARGB{0xFF0B0714},
        StackPanel {
            orientation.horizontal,
            spacing = 28.0,
            hAlign.center,

            // Кольцо и бокал в нём: светятся обводка кольца и линии картинки.
            Grid {
                Ellipse {
                    width = 86,
                    height = 86,
                    stroke = ARGB{0xFFB8FBFF},
                    strokeThickness = 4.0,
                    HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 18.0f},
                },
                Image {
                    source = u"Assets/Coctail.png",
                    width = 50,
                    height = 50,
                    HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 10.0f},
                },
            },

            // Рамка со скруглением и надпись в ней: ореол носят оба, каждый
            // свой.
            Grid {
                Rectangle {
                    width = 230,
                    height = 86,
                    radiusX = 16,
                    radiusY = 16,
                    stroke = ARGB{0xFFFFC4F6},
                    strokeThickness = 4.0,
                    HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 18.0f},
                },
                TextBlock {
                    u"Night Club",
                    fontFamily = u"Assets/neonderthaw.ttf#NeonDerthaw",
                    fontSize = 40,
                    renderTransformOrigin = {0.5, 0.5},
                    renderTransform = RotateTransform {angle = -8.0},
                    hAlign.center,
                    vAlign.center,
                    foreground = ARGB{0xFFFFE8FB},
                    HaloEffect {color = ARGB{0xFFFF2BD6}, blurRadius = 14.0f},
                },
            },

            // Залитая лампочка: два ореола, ядро и разлёт.
            Ellipse {
                width = 28,
                height = 28,
                vAlign.center,
                fill = ARGB{0xFFFFF6B0},
                HaloEffect {color = ARGB{0xFFFFC400}, blurRadius = 12.0f},
                HaloEffect {color = ARGB{0xFFFF7A00}, blurRadius = 36.0f},
            },
        },
    };
}

constexpr effects::Sample samples[] = {
    {u"Как в калькуляторе: табло LCD", lcdCode, &lcd},
    {u"Светодиодный семисегментный индикатор", ledCode, &led},
    {u"Просто светящаяся надпись", glowCode, &glow},
    {u"Неоновая вывеска из фигур", shapesCode, &shapes},
};

}  // namespace

wxl::FrameworkElement effects::haloPage() {
    return showcase(description, samples);
}
