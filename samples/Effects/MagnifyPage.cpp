// Страница эффекта Magnify: описание, примеры и их исходники в общей раскладке
// (Showcase.cpp).
//
// Код примера лежит литералом непосредственно над самим примером: правка
// одного видна рядом с другим.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// ---- Описание -------------------------------------------------------------

constexpr const wchar_t* description = LR"HTML(
<h2>Magnify Effect — рост под указателем</h2>
<p>Элемент растёт, пока над ним указатель: до масштаба из числа и обратно,
за постоянное время. Число меньше 1 — элемент под указателем сжимается.
Пишется в скобках самого элемента.</p>
<pre>Button {
    u"Наведи на меня",
    MagnifyEffect {1.2},
}</pre>
<p><b>Движение считает композитор.</b> Показанный масштаб — это число в
наборе свойств визуала, которое ведёт ключевая анимация, а визуал читает его
выражением. Ни замеров, ни прохода раскладки на кадр; UI-потока касаются
только два события указателя. Масштаб — свойство визуала, а не раскладки:
соседи не сдвигаются, и элемент сохраняет место, под которое его измерили.</p>
<p><b>Чёткость там, где движение кончается.</b> Масштаб визуала растягивает
уже нарисованное, а масштаб, применённый самим XAML (<code>ScaleTransform</code>
в <code>RenderTransform</code>), XAML и рисует — штрихи букв ложатся на
пиксели. Поэтому масштаб разделён надвое: база, в которой рисует XAML, и
масштаб визуала, делённый на неё. При наведении база сразу становится
конечным масштабом, а визуал стартует с обратной ему величины, так что
размер на экране не меняется, и растёт ровно до 1 — в конце это чистая
отрисовка XAML. При уходе наоборот, и в покое элемент снова нарисован один к
одному. Пересчёт пикселей остаётся только в начале движения.</p>
<p>Кроме короткой записи, эффект настраивается теми же тегами, что и
остальной словарь: <code>scale</code> (число или <code>Size</code> по осям),
<code>maximum</code> и <code>minimum</code> — дальние точки, куда движение
заходит перед тем, как вернуться: <code>maximum</code> — при наведении,
<code>minimum</code> — при уходе указателя. Без них движение одно: 140 мс
при наведении и 180 при уходе; с ними это первая фаза, а за ней более
медленный возврат — 200 и 220 мс. <code>duration</code> — длина первой фазы, <code>delayTime</code> —
задержка перед ростом. Эффект — ручка, как любая обёртка: один, построенный
заранее, носит вся панель инструментов ниже, и анимации у кнопок общие.</p>
<p>У панели пик 1.35 и дно 0.95; бокал, наоборот, при наведении сжимается:
<code>MagnifyEffect {0.8, maximum = 0.7, minimum = 1.2}</code> — ныряет до 0.7
и встаёт на 0.8, а при уходе подпрыгивает до 1.2 и возвращается к 1. У первой
кнопки перелёта нет — с ней сравнивается эталон.</p>
<p>Зависимость масштаба от расстояния до центра пробовали и убрали: событий
мыши приходит несколько штук на проход, размер между ними прыгал, и оставалось
неподвижное промежуточное состояние, где чёткости быть не может.</p>
<p>Эффекты складываются: бокал ниже носит и ореол, и масштаб — ореол меняется
вместе с ним.</p>
)HTML";

// ---- Примеры --------------------------------------------------------------

// Кнопка с эффектом и рядом эталон: такая же кнопка, увеличенная раз и
// навсегда средствами XAML, чтобы сравнить обе в одинаковом состоянии.
// Размер у обеих задан так, что при масштабе 1.2 угол от центра уходит на
// целые 17 и 5 пикселей: иначе эталон растеризуется в дробной позиции и
// попиксельно с эффектом не совпадёт.
constexpr const wchar_t* buttonCode = LR"CODE(
StackPanel {
    orientation.horizontal,
    spacing = 64.0,
    hAlign.center,

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
)CODE";

FrameworkElement button() {
    return StackPanel {
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
    };
}

// Панель инструментов: у каждой кнопки свой цвет, а эффект один на всех.
constexpr const wchar_t* toolbarCode = LR"CODE(
// Один эффект на всю панель: копии — это тот же эффект, и анимации у
// всех кнопок общие.
auto const pop = MagnifyEffect {1.2, maximum = 1.35, minimum = 0.95};

auto const tool = [pop](FluentSymbol glyph, ARGB tint) {
    return Button {
        content = SymbolIcon {symbol = glyph, foreground = tint},
        width = 52,
        height = 52,
        pop,
    };
};

StackPanel {
    orientation.horizontal,
    spacing = 22.0,
    hAlign.center,
    tool(FluentSymbol::Home, ARGB{0xFF0F6CBD}),
    tool(FluentSymbol::Mail, ARGB{0xFFD83B01}),
    tool(FluentSymbol::Camera, ARGB{0xFF8764B8}),
    tool(FluentSymbol::MusicNote, ARGB{0xFFE3008C}),
    tool(FluentSymbol::Globe, ARGB{0xFF107C10}),
}
)CODE";

FrameworkElement toolbar() {
    // Один эффект на всю панель: копии — это тот же эффект, и анимации у
    // всех кнопок общие.
    auto const pop = MagnifyEffect {1.2, maximum = 1.35, minimum = 0.95};

    auto const tool = [pop](FluentSymbol glyph, ARGB tint) {
        return Button {
            content = SymbolIcon {symbol = glyph, foreground = tint},
            width = 52,
            height = 52,
            pop,
        };
    };

    return StackPanel {
        orientation.horizontal,
        spacing = 22.0,
        hAlign.center,
        Margin {0, 20},
        tool(FluentSymbol::Home, ARGB{0xFF0F6CBD}),
        tool(FluentSymbol::Mail, ARGB{0xFFD83B01}),
        tool(FluentSymbol::Camera, ARGB{0xFF8764B8}),
        tool(FluentSymbol::MusicNote, ARGB{0xFFE3008C}),
        tool(FluentSymbol::Globe, ARGB{0xFF107C10}),
    };
}

// Неоновый бокал: ореол и сжатие под указателем на одной картинке.
constexpr const wchar_t* neonCode = LR"CODE(
Border {
    CornerRadius {8},
    Padding {24, 28},
    background = ARGB{0xFF0B0714},
    Image {
        source = u"Assets/Coctail.png",
        width = 72,
        height = 72,
        HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 12.0f},
        MagnifyEffect {0.8, maximum = 0.7, minimum = 1.2},
    },
}
)CODE";

FrameworkElement neon() {
    return Border {
        CornerRadius {8},
        Padding {24, 28},
        background = ARGB{0xFF0B0714},
        Image {
            source = u"Assets/Coctail.png",
            width = 72,
            height = 72,
            HaloEffect {color = ARGB{0xFF00C8FF}, blurRadius = 12.0f},
            MagnifyEffect {0.8, maximum = 0.7, minimum = 1.2},
        },
    };
}

constexpr effects::Sample samples[] = {
    {u"Слева с эффектом, справа эталон ×1.2", buttonCode, &button},
    {u"Панель инструментов", toolbarCode, &toolbar},
    {u"Неоновый бокал: ореол и сжатие вместе", neonCode, &neon},
};

}  // namespace

wxl::FrameworkElement effects::magnifyPage() {
    return showcase(description, samples);
}
