// Страница эффекта Magnify: описание, примеры и их исходники в общей раскладке
// (Showcase.cpp).
//
// Код каждого примера — файл в Snippets/<эффект>/: он включается по #include
// прямо в функцию, которая строит пример, и он же, вшитый байтами, показывается
// справа. `X.h.embed` — то, что вернул бы `#embed "X.h"`; его делает CMake,
// пока MSVC не умеет #embed. Показанное и работающее — один и тот же текст,
// разойтись им негде.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// ---- Описание -------------------------------------------------------------

// Описание — Snippets/Magnify/page.html: HTML отдельным файлом, чтобы править
// его и смотреть в браузере как есть.
constexpr char8_t description[] = {
#include "Snippets/Magnify/page.html.embed"
};

// ---- Примеры --------------------------------------------------------------

// Кнопка с эффектом и рядом эталон: такая же кнопка, увеличенная раз и
// навсегда средствами XAML, чтобы сравнить обе в одинаковом состоянии.
// Размер у обеих задан так, что при масштабе 1.2 угол от центра уходит на
// целые 17 и 5 пикселей: иначе эталон растеризуется в дробной позиции и
// попиксельно с эффектом не совпадёт.
constexpr char8_t buttonText[] = {
#include "Snippets/Magnify/Button.h.embed"
};

FrameworkElement button() {
    return
#include "Snippets/Magnify/Button.h"
    ;
}

// Панель инструментов: у каждой кнопки свой цвет, а эффект один на всех.
constexpr char8_t toolbarPopText[] = {
#include "Snippets/Magnify/ToolbarPop.h.embed"
};
constexpr char8_t toolbarText[] = {
#include "Snippets/Magnify/Toolbar.h.embed"
};

FrameworkElement toolbar() {
#include "Snippets/Magnify/ToolbarPop.h"

    return
#include "Snippets/Magnify/Toolbar.h"
    ;
}

// Неоновый бокал: ореол и сжатие под указателем на одной картинке.
constexpr char8_t cocktailText[] = {
#include "Snippets/Magnify/Cocktail.h.embed"
};

FrameworkElement neon() {
    return
#include "Snippets/Magnify/Cocktail.h"
    ;
}

constexpr effects::Sample samples[] = {
    {u"Слева с эффектом, справа эталон ×1.2", {effects::snippet(buttonText)}, &button},
    {u"Панель инструментов", {effects::snippet(toolbarPopText), effects::snippet(toolbarText)}, &toolbar},
    {u"Неоновый бокал: ореол и сжатие вместе", {effects::snippet(cocktailText)}, &neon},
};

}  // namespace

wxl::FrameworkElement effects::magnifyPage() {
    return showcase(effects::snippet(description), samples);
}
