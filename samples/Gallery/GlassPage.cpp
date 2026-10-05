// Страница эффекта Glass — перенесена из приложения Effects: описание, примеры и их исходники в духе Gallery.
//
// Код каждого примера — файл в Snippets/Glass/: он включается по #include в функцию, которая строит пример, и он же,
// вшитый байтами, показывается под примером. Показанное и работающее — один и тот же текст.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t FrostHeader[] = {
#include "Snippets/Glass/Frost.html.embed"
};
constexpr char8_t FrostCode[] = {
#include "Snippets/Glass/Frost.h.embed"
};

FrameworkElement frostExample() {
#include "Snippets/Glass/Frost.h"

    return gallery::controlExample({
        .header = gallery::snippet(FrostHeader),
        .example = example,
        .code = gallery::snippet(FrostCode),
    });
}

constexpr char8_t TintedHeader[] = {
#include "Snippets/Glass/Tinted.html.embed"
};
constexpr char8_t TintedCode[] = {
#include "Snippets/Glass/Tinted.h.embed"
};

FrameworkElement tintedExample() {
#include "Snippets/Glass/Tinted.h"

    return gallery::controlExample({
        .header = gallery::snippet(TintedHeader),
        .example = example,
        .code = gallery::snippet(TintedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::glassPage() {
    return StackPanel {frostExample(), tintedExample()};
}
