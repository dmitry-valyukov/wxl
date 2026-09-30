// Страница Viewbox — ViewboxPage оригинала: один пример.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t contentHeader[] = {
#include "Snippets/Viewbox/ContentInsideViewbox.html.embed"
};
constexpr char8_t contentCode[] = {
#include "Snippets/Viewbox/ContentInsideViewbox.h.embed"
};

FrameworkElement insideViewbox() {
#include "Snippets/Viewbox/ContentInsideViewbox.h"

    return gallery::controlExample({
        .header = gallery::snippet(contentHeader),
        .example = viewbox,
        .options = {StackPanel {width = 200, size, stretchGroup, directionGroup}},
        .code = gallery::snippet(contentCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::viewboxPage() {
    return StackPanel {insideViewbox()};
}