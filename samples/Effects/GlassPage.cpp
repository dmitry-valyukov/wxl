// Страница эффекта Glass: описание, примеры и их исходники. Раскладка
// страницы общая у всех эффектов (Showcase.cpp); здесь только то, что своё
// у Glass. Код примера включается и в функцию, и байтами для показа — см.
// HaloPage.cpp.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t description[] = {
#include "Snippets/Glass/page.html.embed"
};

// Карточка на меловой доске: полупрозрачная заливка поверх стекла.
constexpr char8_t frostText[] = {
#include "Snippets/Glass/Frost.h.embed"
};

FrameworkElement frost() {
    return
#include "Snippets/Glass/Frost.h"
    ;
}

// Тинт стекла: тёплый, тёмный и без тинта, с малым радиусом.
constexpr char8_t tintedText[] = {
#include "Snippets/Glass/Tinted.h.embed"
};

FrameworkElement tinted() {
    return
#include "Snippets/Glass/Tinted.h"
    ;
}

constexpr effects::Sample samples[] = {
    {u"Карточка на меловой доске", {effects::snippet(frostText)}, &frost},
    {u"Тинт: тёплый, тёмный, никакой", {effects::snippet(tintedText)}, &tinted},
};

}  // namespace

wxl::FrameworkElement effects::glassPage() {
    return showcase(effects::snippet(description), samples);
}
