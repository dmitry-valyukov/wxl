// Страница SplitButton — SplitButtonPage оригинала: два примера.
//
// Флайут оригинала — GridView с ItemsWrapGrid в ItemsPanelTemplate; шаблон
// панели здесь не построить, и три колонки даёт ширина GridView.

#include "Pages.h"
#include "event_awaitable.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t colorHeader[] = {
#include "Snippets/SplitButton/ColorPicker.html.embed"
};
constexpr char8_t colorCode[] = {
#include "Snippets/SplitButton/ColorPicker.h.embed"
};

FrameworkElement colorExample() {
#include "Snippets/SplitButton/ColorPicker.h"

    return gallery::controlExample({
        .header = gallery::snippet(colorHeader),
        .example = Grid {columnSpacing = 24.0, colorButton},
        .options = {richBox},
        .code = gallery::snippet(colorCode),
    });
}

constexpr char8_t textHeader[] = {
#include "Snippets/SplitButton/Text.html.embed"
};
constexpr char8_t textCode[] = {
#include "Snippets/SplitButton/Text.h.embed"
};

FrameworkElement textExample() {
#include "Snippets/SplitButton/Text.h"

    return gallery::controlExample({
        .header = gallery::snippet(textHeader),
        .example = chooser,
        .code = gallery::snippet(textCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::splitButtonPage() {
    return StackPanel {colorExample(), textExample()};
}