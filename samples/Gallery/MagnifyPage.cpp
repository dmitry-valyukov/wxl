// Страница эффекта Magnify — перенесена из приложения Effects: описание, примеры и их исходники в духе Gallery.
//
// Код каждого примера — файл в Snippets/Magnify/: он включается по #include в функцию, которая строит пример, и он же,
// вшитый байтами, показывается под примером. Показанное и работающее — один и тот же текст.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t ButtonHeader[] = {
#include "Snippets/Magnify/Button.html.embed"
};
constexpr char8_t ButtonCode[] = {
#include "Snippets/Magnify/Button.h.embed"
};

FrameworkElement buttonExample() {
#include "Snippets/Magnify/Button.h"

    return gallery::controlExample({
        .header = gallery::snippet(ButtonHeader),
        .example = example,
        .code = gallery::snippet(ButtonCode),
    });
}

constexpr char8_t ToolbarHeader[] = {
#include "Snippets/Magnify/Toolbar.html.embed"
};
constexpr char8_t ToolbarCode[] = {
#include "Snippets/Magnify/Toolbar.h.embed"
};

FrameworkElement toolbarExample() {
#include "Snippets/Magnify/Toolbar.h"

    return gallery::controlExample({
        .header = gallery::snippet(ToolbarHeader),
        .example = example,
        .code = gallery::snippet(ToolbarCode),
    });
}

constexpr char8_t CocktailHeader[] = {
#include "Snippets/Magnify/Cocktail.html.embed"
};
constexpr char8_t CocktailCode[] = {
#include "Snippets/Magnify/Cocktail.h.embed"
};

FrameworkElement cocktailExample() {
#include "Snippets/Magnify/Cocktail.h"

    return gallery::controlExample({
        .header = gallery::snippet(CocktailHeader),
        .example = example,
        .code = gallery::snippet(CocktailCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::magnifyPage() {
    return StackPanel {buttonExample(), toolbarExample(), cocktailExample()};
}
