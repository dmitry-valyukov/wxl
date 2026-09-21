// Страница эффекта Halo: описание, примеры и их исходники. Раскладка страницы
// общая у всех эффектов (Showcase.cpp); здесь только то, что своё у Halo.
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

// Описание — Snippets/Halo/page.html: HTML отдельным файлом, чтобы править
// его и смотреть в браузере как есть.
constexpr char8_t description[] = {
#include "Snippets/Halo/page.html.embed"
};

// ---- Примеры --------------------------------------------------------------

// Табло калькулятора: тёмно-зелёное свечение на оливковой подложке LCD.
constexpr char8_t lcdText[] = {
#include "Snippets/Halo/Lcd.h.embed"
};

FrameworkElement lcd() {
    return
#include "Snippets/Halo/Lcd.h"
    ;
}

// Светодиодный семисегментник: горячее ядро и красно-оранжевый разлёт.
constexpr char8_t ledSegmentText[] = {
#include "Snippets/Halo/LedSegment.h.embed"
};
constexpr char8_t ledText[] = {
#include "Snippets/Halo/Led.h.embed"
};

FrameworkElement led() {
#include "Snippets/Halo/LedSegment.h"

    return
#include "Snippets/Halo/Led.h"
    ;
}

// Просто светящаяся надпись: обычная гарнитура, один ореол.
constexpr char8_t glowText[] = {
#include "Snippets/Halo/Glow.h.embed"
};

FrameworkElement glow() {
    return
#include "Snippets/Halo/Glow.h"
    ;
}

// Неоновая вывеска из фигур: ореол носит и фигура, не только текст.
constexpr char8_t neonSignText[] = {
#include "Snippets/Halo/NeonSign.h.embed"
};

FrameworkElement shapes() {
    return
#include "Snippets/Halo/NeonSign.h"
    ;
}

constexpr effects::Sample samples[] = {
    {u"Как в калькуляторе: табло LCD", {effects::snippet(lcdText)}, &lcd},
    {u"Светодиодный семисегментный индикатор", {effects::snippet(ledSegmentText), effects::snippet(ledText)}, &led},
    {u"Просто светящаяся надпись", {effects::snippet(glowText)}, &glow},
    {u"Неоновая вывеска из фигур", {effects::snippet(neonSignText)}, &shapes},
};

}  // namespace

wxl::FrameworkElement effects::haloPage() {
    return showcase(effects::snippet(description), samples);
}
