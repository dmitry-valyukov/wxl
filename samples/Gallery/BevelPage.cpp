// Страница эффекта Bevel — перенесена из приложения Effects: описание, примеры и их исходники в духе Gallery.
//
// Код каждого примера — файл в Snippets/Bevel/: он включается по #include в функцию, которая строит пример, и он же,
// вшитый байтами, показывается под примером. Показанное и работающее — один и тот же текст.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t CalcPanelHeader[] = {
#include "Snippets/Bevel/CalcPanel.html.embed"
};
constexpr char8_t CalcPanelCode[] = {
#include "Snippets/Bevel/CalcPanel.h.embed"
};

FrameworkElement calcPanelExample() {
#include "Snippets/Bevel/CalcPanel.h"

    return gallery::controlExample({
        .header = gallery::snippet(CalcPanelHeader),
        .example = example,
        .code = gallery::snippet(CalcPanelCode),
    });
}

constexpr char8_t StretchHeader[] = {
#include "Snippets/Bevel/Stretch.html.embed"
};
constexpr char8_t StretchCode[] = {
#include "Snippets/Bevel/Stretch.h.embed"
};

FrameworkElement stretchExample() {
#include "Snippets/Bevel/Stretch.h"

    return gallery::controlExample({
        .header = gallery::snippet(StretchHeader),
        .example = example,
        .code = gallery::snippet(StretchCode),
    });
}

constexpr char8_t KeypadHeader[] = {
#include "Snippets/Bevel/Keypad.html.embed"
};
constexpr char8_t KeypadCode[] = {
#include "Snippets/Bevel/Keypad.h.embed"
};

FrameworkElement keypadExample() {
#include "Snippets/Bevel/Keypad.h"

    return gallery::controlExample({
        .header = gallery::snippet(KeypadHeader),
        .example = example,
        .code = gallery::snippet(KeypadCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::bevelPage() {
    return StackPanel {calcPanelExample(), stretchExample(), keypadExample()};
}
