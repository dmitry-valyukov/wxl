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
<p>Элемент растёт, когда над ним указатель, и тем сильнее, чем ближе тот к
центру. Внутри овала с пропорциями элемента и вдвое меньшего — до
<code>maxScale</code>, так что целиться надо в область, а не в точку; от овала
к краям масштаб плавно спадает до обычного. Пишется в скобках самого
элемента.</p>
<pre>Button {
    u"Наведи на меня",
    MagnifyEffect {1.2f},
}</pre>
<p><b>На потоке интерфейса не выполняется ничего.</b> XAML сам держит
положение указателя над элементом в наборе свойств композиции
(<code>GetPointerPositionPropertySet</code>), а выражение на визуале элемента
пересчитывает его в масштаб — кадр за кадром, внутри композитора, без
обработчика <code>PointerMoved</code> и без прохода раскладки.</p>
<p>Масштаб и его центр — свойства визуала, а не раскладки: соседи не
сдвигаются, и элемент сохраняет место, под которое его измерили.</p>
<p>Уйдя, указатель оставляет в наборе последнее положение, поэтому выражение
умножается на собственный вес наведения эффекта: он плавно поднимается до 1
на <code>PointerEntered</code> и опускается до 0 на <code>PointerExited</code>,
и элемент возвращается к размеру, а не застывает там, где указатель пересёк
край.</p>
<p><b>Чётко и в увеличенном виде.</b> Масштаб визуала растягивает то, что
XAML уже нарисовал, и текст с иконками при росте мылится. Поэтому, пока
указатель над элементом, его <code>RasterizationScale</code> поднят до
<code>maxScale</code>: XAML один раз перерисовывает элемент и детей настолько
же плотнее, а композитор этот растр только сжимает. Обратно к 1 он
возвращается, когда элемент уже сжался, — в покое элемент нарисован в родном
разрешении.</p>
<p>Эффекты складываются: бокал ниже носит и ореол, и рост — ореол растёт
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
        MagnifyEffect {1.2f},
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
            MagnifyEffect {1.2f},
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

// Панель инструментов: у каждой кнопки свой рост.
constexpr const wchar_t* toolbarCode = LR"CODE(
auto const tool = [](FluentSymbol glyph) {
    return Button {
        content = SymbolIcon {symbol = glyph},
        width = 52,
        height = 52,
        MagnifyEffect {1.2f},
    };
};

StackPanel {
    orientation.horizontal,
    spacing = 22.0,
    hAlign.center,
    tool(FluentSymbol::Home),
    tool(FluentSymbol::Mail),
    tool(FluentSymbol::Camera),
    tool(FluentSymbol::MusicNote),
    tool(FluentSymbol::Globe),
}
)CODE";

FrameworkElement toolbar() {
    auto const tool = [](FluentSymbol glyph) {
        return Button {
            content = SymbolIcon {symbol = glyph},
            width = 52,
            height = 52,
            MagnifyEffect {1.2f},
        };
    };

    return StackPanel {
        orientation.horizontal,
        spacing = 22.0,
        hAlign.center,
        Margin {0, 20},
        tool(FluentSymbol::Home),
        tool(FluentSymbol::Mail),
        tool(FluentSymbol::Camera),
        tool(FluentSymbol::MusicNote),
        tool(FluentSymbol::Globe),
    };
}

// Неоновый бокал: ореол и рост на одной картинке.
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
        MagnifyEffect {1.2f},
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
            MagnifyEffect {1.2f},
        },
    };
}

constexpr effects::Sample samples[] = {
    {u"Слева с эффектом, справа эталон ×1.2", buttonCode, &button},
    {u"Панель инструментов", toolbarCode, &toolbar},
    {u"Неоновый бокал: ореол и рост вместе", neonCode, &neon},
};

}  // namespace

wxl::FrameworkElement effects::magnifyPage() {
    return showcase(description, samples);
}
