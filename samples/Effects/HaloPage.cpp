// Страница эффекта Halo: описание сверху, примеры слева, исходник показанного
// примера справа. Все три части прокручиваются сами по себе.
//
// Код примера лежит литералом непосредственно над самим примером: правка
// одного видна рядом с другим. Форматировать его не понадобилось — [code=cpp]
// разбирает RsdnBlock, подложку даёт <pre> темы, подсветку — wxl.highlight.

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
    L"0",
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

// Пример: подпись, живой показ и его исходник. Литерал кода стоит рядом с
// кодом, который он показывает.
struct Sample {
    const wchar_t* title;
    const wchar_t* code;
    FrameworkElement (*build)();
};

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
        L"1234.56",
        fontFamily = L"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        FontWeight {600},
        CharacterSpacing {75},
        textAlignment.right,
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
            L"1234.56",
            fontFamily = L"Assets/digitalism.ttf#Digitalism",
            fontSize = 44,
            FontWeight {600},
            CharacterSpacing {75},
            textAlignment.right,
            foreground = ARGB{0xFF2C3A1C},
            HaloEffect {color = ARGB{0xFF5C8A20}, blurRadius = 14.0f},
        },
    };
}

// Светодиодный семисегментник: горячее ядро и красно-оранжевый разлёт.
constexpr const wchar_t* ledCode = LR"CODE(
// Общий вид разряда: и погашенного, и горящего.
auto const segment = Preset {
    fontFamily = L"Assets/digitalism.ttf#Digitalism",
    fontSize = 44,
    CharacterSpacing {75},
    textAlignment.right,
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
        TextBlock {L"88:88", segment, foreground = ARGB{0x14FF4A00}},

        // Два ореола на одном элементе: тугое ядро и широкий разлёт.
        TextBlock {
            L"12:34",
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
        fontFamily = L"Assets/digitalism.ttf#Digitalism",
        fontSize = 44,
        CharacterSpacing {75},
        textAlignment.right,
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
            TextBlock {L"88:88", segment, foreground = ARGB{0x14FF4A00}},

            // Два ореола на одном элементе: тугое ядро и широкий разлёт.
            TextBlock {
                L"12:34",
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
        L"wxl",
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
            L"wxl",
            fontSize = 56,
            FontWeight {700},
            CharacterSpacing {80},
            hAlign.center,
            foreground = ARGB{0xFFEAF7FF},
            HaloEffect {color = ARGB{0xFF2BB3F3}, blurRadius = 24.0f},
        },
    };
}

// Ореол на размеченном тексте — этого ещё нет.
constexpr const wchar_t* markupCode = LR"CODE(
// TODO: need to implement.
//
// Ореол носит всё, что умеет отдать альфу своих глифов:
//
//     requires { element.getAlphaMask(); }
//
// У HtmlBlock этого пока нет: RichTextBlock.GetAlphaMask в профиль не входит.
)CODE";

FrameworkElement markup() {
    return Border {
        CornerRadius {8},
        Padding {18, 14},
        background = ARGB{0xFF1A1206},
        borderBrush = ARGB{0xFF6B4A12},
        BorderThickness {1},
        TextBlock {
            L"TODO: need to implement.",
            fontSize = 16,
            FontWeight {600},
            foreground = ARGB{0xFFE8C980},
        },
    };
}

constexpr Sample samples[] = {
    {L"Как в калькуляторе: табло LCD", lcdCode, &lcd},
    {L"Светодиодный семисегментный индикатор", ledCode, &led},
    {L"Просто светящаяся надпись", glowCode, &glow},
    {L"HaloEffect на HtmlBlock", markupCode, &markup},
};

// Код для правой половины. Разметка RSDN берёт тело [code] буквально, так что
// экранировать в нём нечего; язык включает подсветку wxl.highlight.
std::wstring codeMarkup(const wchar_t* code) {
    std::wstring_view body{code};
    while (!body.empty() && (body.front() == L'\n' || body.front() == L'\r')) {
        body.remove_prefix(1);
    }
    while (!body.empty() && (body.back() == L'\n' || body.back() == L'\r')) {
        body.remove_suffix(1);
    }
    return L"[code=cpp]" + std::wstring{body} + L"[/code]";
}

}  // namespace

wxl::FrameworkElement effects::haloPage() {
    // Правая половина: исходник того примера, чью кнопку нажали последней.
    auto code = RsdnBlock {
        isTextSelectionEnabled = true,
        Margin {16, 12},
        L"[i]Нажмите «Показать код» под любым примером.[/i]",
    };

    auto left = StackPanel {
        spacing = 16.0,
        Margin {16, 12},
    };

    for (auto const& sample : samples) {
        left.children().append(Card {
            Padding {16},
            StackPanel {
                spacing = 10.0,
                TextBlock {
                    sample.title,
                    fontSize = 16,
                    FontWeight {600},
                },
                sample.build(),
                Button {
                    content = L"Показать код",
                    toolTip = L"Показать справа исходник этого примера",
                    hAlign.left,
                    onClick = [code, text = sample.code] { code.rsdn(codeMarkup(text)); },
                },
            },
        });
    }

    return Grid {
        rowDefinitions = L"2*,3*",

        ScrollViewer {
            row = 0,
            content = HtmlBlock {
                isTextSelectionEnabled = true,
                Margin {20, 14},
                description,
            },
        },

        Border {
            row = 1,
            BorderThickness {0, 1, 0, 0},
            borderBrush = brushes.Card.StrokeColorDefault,
            Columns {
                ScrollViewer {content = left},
                Border {
                    BorderThickness {1, 0, 0, 0},
                    borderBrush = brushes.Card.StrokeColorDefault,
                    ScrollViewer {content = code},
                },
            },
        },
    };
}
