// Страница эффекта Button3D — стенд клавиши: описание, клавиатура и панель
// настроек, которая пишет в сеттеры эффекта.
//
// Код примера — файл в Snippets/Button3D/: он включается по #include в функцию,
// которая строит пример, и он же, вшитый байтами, показывается под примером.
// Показанное и работающее — один и тот же текст.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t KeypadHeader[] = {
#include "Snippets/Button3D/Keypad.html.embed"
};
constexpr char8_t KeypadCode[] = {
#include "Snippets/Button3D/Keypad.h.embed"
};

FrameworkElement keypadExample() {
#include "Snippets/Button3D/Keypad.h"

    return gallery::controlExample({
        .header = gallery::snippet(KeypadHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(KeypadCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::button3DPage() {
    return StackPanel {keypadExample()};
}
