// Страница WrapPanel -- WrapPanelPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/WrapPanel/Basic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/WrapPanel/Basic.h.embed"
};

FrameworkElement basic() {
#include "Snippets/WrapPanel/Basic.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = layoutHost,
        .options = {options},
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t stretchHeader[] = {
#include "Snippets/WrapPanel/Stretch.html.embed"
};
constexpr char8_t stretchCode[] = {
#include "Snippets/WrapPanel/Stretch.h.embed"
};

FrameworkElement lastItem() {
#include "Snippets/WrapPanel/Stretch.h"

    return gallery::controlExample({
        .header = gallery::snippet(stretchHeader),
        .example = panelHost,
        .options = {stretchToggle},
        .code = gallery::snippet(stretchCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::wrapPanelPage() {
    return StackPanel {basic(), lastItem()};
}
