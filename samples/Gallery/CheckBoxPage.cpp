// Страница CheckBox — CheckBoxPage оригинала: два, три состояния и «выбрать всё».

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t twoHeader[] = {
#include "Snippets/CheckBox/TwoState.html.embed"
};
constexpr char8_t twoCode[] = {
#include "Snippets/CheckBox/TwoState.h.embed"
};

FrameworkElement twoState() {
#include "Snippets/CheckBox/TwoState.h"

    return gallery::controlExample({
        .header = gallery::snippet(twoHeader),
        .example = StackPanel {orientation.horizontal, box},
        .output = {output},
        .code = gallery::snippet(twoCode),
    });
}

constexpr char8_t threeHeader[] = {
#include "Snippets/CheckBox/ThreeState.html.embed"
};
constexpr char8_t threeCode[] = {
#include "Snippets/CheckBox/ThreeState.h.embed"
};

FrameworkElement threeState() {
#include "Snippets/CheckBox/ThreeState.h"

    return gallery::controlExample({
        .header = gallery::snippet(threeHeader),
        .example = StackPanel {orientation.horizontal, box},
        .output = {output},
        .code = gallery::snippet(threeCode),
    });
}

constexpr char8_t allHeader[] = {
#include "Snippets/CheckBox/SelectAll.html.embed"
};
constexpr char8_t allCode[] = {
#include "Snippets/CheckBox/SelectAll.h.embed"
};

FrameworkElement selectAll() {
#include "Snippets/CheckBox/SelectAll.h"

    return gallery::controlExample({
        .header = gallery::snippet(allHeader),
        .example = options,
        .code = gallery::snippet(allCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::checkBoxPage() {
    return StackPanel {twoState(), threeState(), selectAll()};
}