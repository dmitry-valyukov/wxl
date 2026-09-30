// Страница DropDownButton — DropDownButtonPage оригинала: два примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/DropDownButton/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/DropDownButton/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/DropDownButton/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t iconHeader[] = {
#include "Snippets/DropDownButton/Icon.html.embed"
};
constexpr char8_t iconCode[] = {
#include "Snippets/DropDownButton/Icon.h.embed"
};

FrameworkElement withIcon() {
    return gallery::controlExample({
        .header = gallery::snippet(iconHeader),
        .example =
#include "Snippets/DropDownButton/Icon.h"
        ,
        .code = gallery::snippet(iconCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::dropDownButtonPage() {
    return StackPanel {simple(), withIcon()};
}