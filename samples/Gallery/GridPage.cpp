// Страница Grid — GridPage оригинала: один пример.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t threeHeader[] = {
#include "Snippets/Grid/ThreeByThreeGrid.html.embed"
};
constexpr char8_t threeCode[] = {
#include "Snippets/Grid/ThreeByThreeGrid.h.embed"
};

FrameworkElement three() {
#include "Snippets/Grid/ThreeByThreeGrid.h"

    return gallery::controlExample({
        .header = gallery::snippet(threeHeader),
        .example = grid,
        .options = {options},
        .code = gallery::snippet(threeCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::gridPage() {
    return StackPanel {three()};
}