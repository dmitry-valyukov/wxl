// Страница эффекта Gaussian Blur: описание, примеры и их исходники. Раскладка
// страницы общая у всех эффектов (Showcase.cpp); здесь только то, что своё у
// этого эффекта. Как устроены сниппеты — в HaloPage.cpp.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

// ---- Описание -------------------------------------------------------------

constexpr char8_t description[] = {
#include "Snippets/GaussianBlur/page.html.embed"
};

// ---- Примеры --------------------------------------------------------------

// Клавиша калькулятора: цифра с тугой тёмной обводкой одним эффектом.
constexpr char8_t keycapText[] = {
#include "Snippets/GaussianBlur/Keycap.h.embed"
};

FrameworkElement keycap() {
    return
#include "Snippets/GaussianBlur/Keycap.h"
    ;
}

// Рядом: ореол тенью, тот же ореол графом при гамме 1 и при гамме 0.4.
constexpr char8_t compareText[] = {
#include "Snippets/GaussianBlur/Compare.h.embed"
};

FrameworkElement compare() {
    return
#include "Snippets/GaussianBlur/Compare.h"
    ;
}

// Три гаммы одного ореола: плотный до края, как есть, слабая корона.
constexpr char8_t gammaText[] = {
#include "Snippets/GaussianBlur/Gamma.h.embed"
};

FrameworkElement gammas() {
    return
#include "Snippets/GaussianBlur/Gamma.h"
    ;
}

// zIndex: слой под глифами и тот же слой над ними.
constexpr char8_t onTopText[] = {
#include "Snippets/GaussianBlur/OnTop.h.embed"
};

FrameworkElement onTop() {
    return
#include "Snippets/GaussianBlur/OnTop.h"
    ;
}

constexpr effects::Sample samples[] = {
    {u"Как в калькуляторе: цифра клавиши", {effects::snippet(keycapText)}, &keycap},
    {u"Рядом с Halo: та же тень и гамма", {effects::snippet(compareText)}, &compare},
    {u"Три гаммы одного ореола", {effects::snippet(gammaText)}, &gammas},
    {u"zIndex: под глифами и над ними", {effects::snippet(onTopText)}, &onTop},
};

}  // namespace

wxl::FrameworkElement effects::gaussianBlurPage() {
    return showcase(effects::snippet(description), samples);
}
