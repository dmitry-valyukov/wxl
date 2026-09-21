// Страница эффекта Bevel: описание, примеры и их исходники в общей раскладке
// (Showcase.cpp).
//
// Код каждого примера — файл в Snippets/<эффект>/: он включается по #include
// прямо в функцию, которая строит пример, и он же, вшитый байтами, показывается
// справа. `X.h.embed` — то, что вернул бы `#embed "X.h"`; его делает CMake,
// пока MSVC не умеет #embed.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// ---- Описание -------------------------------------------------------------

// Описание — Snippets/Bevel/page.html: HTML отдельным файлом, чтобы править
// его и смотреть в браузере как есть.
constexpr char8_t description[] = {
#include "Snippets/Bevel/page.html.embed"
};

// ---- Примеры --------------------------------------------------------------

// Клавиша и корпус калькулятора: свободные функции, поэтому их сниппет
// включается здесь, а не в теле функции примера.
#include "Snippets/Bevel/CalcParts.h"

// Панель как у калькулятора: корпус, вдавленное табло, клавиши.
constexpr char8_t calcPartsText[] = {
#include "Snippets/Bevel/CalcParts.h.embed"
};
constexpr char8_t calcPanelText[] = {
#include "Snippets/Bevel/CalcPanel.h.embed"
};

FrameworkElement calcPanel() {
    return
#include "Snippets/Bevel/CalcPanel.h"
    ;
}

// Широкая рамка: эффект рядом с тем же градиентом, написанным руками.
constexpr char8_t stretchText[] = {
#include "Snippets/Bevel/Stretch.h.embed"
};

FrameworkElement stretch() {
    return
#include "Snippets/Bevel/Stretch.h"
    ;
}

// Клавиши разной ширины с одним кантом: перелом у каждой в своих углах.
constexpr char8_t keypadText[] = {
#include "Snippets/Bevel/Keypad.h.embed"
};

FrameworkElement keypad() {
    return
#include "Snippets/Bevel/Keypad.h"
    ;
}

constexpr effects::Sample samples[] = {
    {u"Как в калькуляторе", {effects::snippet(calcPartsText), effects::snippet(calcPanelText)}, &calcPanel},
    {u"Широкая рамка: эффект и градиент от угла к углу", {effects::snippet(stretchText)}, &stretch},
    {u"Клавиши разной ширины, один кант", {effects::snippet(calcPartsText), effects::snippet(keypadText)}, &keypad},
};

}  // namespace

wxl::FrameworkElement effects::bevelPage() {
    return showcase(effects::snippet(description), samples);
}
