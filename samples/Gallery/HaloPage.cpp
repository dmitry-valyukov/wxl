// Страница эффекта Halo — перенесена из приложения Effects: описание, примеры и их исходники в духе Gallery.
//
// Код каждого примера — файл в Snippets/Halo/: он включается по #include в функцию, которая строит пример, и он же,
// вшитый байтами, показывается под примером. Показанное и работающее — один и тот же текст.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t LcdHeader[] = {
#include "Snippets/Halo/Lcd.html.embed"
};
constexpr char8_t LcdCode[] = {
#include "Snippets/Halo/Lcd.h.embed"
};

FrameworkElement lcdExample() {
#include "Snippets/Halo/Lcd.h"

    return gallery::controlExample({
        .header = gallery::snippet(LcdHeader),
        .example = example,
        .code = gallery::snippet(LcdCode),
    });
}

constexpr char8_t LedHeader[] = {
#include "Snippets/Halo/Led.html.embed"
};
constexpr char8_t LedCode[] = {
#include "Snippets/Halo/Led.h.embed"
};

FrameworkElement ledExample() {
#include "Snippets/Halo/Led.h"

    return gallery::controlExample({
        .header = gallery::snippet(LedHeader),
        .example = example,
        .code = gallery::snippet(LedCode),
    });
}

constexpr char8_t GlowHeader[] = {
#include "Snippets/Halo/Glow.html.embed"
};
constexpr char8_t GlowCode[] = {
#include "Snippets/Halo/Glow.h.embed"
};

FrameworkElement glowExample() {
#include "Snippets/Halo/Glow.h"

    return gallery::controlExample({
        .header = gallery::snippet(GlowHeader),
        .example = example,
        .code = gallery::snippet(GlowCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::haloPage() {
    return StackPanel {lcdExample(), ledExample(), glowExample()};
}
