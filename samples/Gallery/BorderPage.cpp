// Страница Border — BorderPage оригинала: один пример.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t aroundHeader[] = {
#include "Snippets/Border/BorderAroundTextblock.html.embed"
};
constexpr char8_t aroundCode[] = {
#include "Snippets/Border/BorderAroundTextblock.h.embed"
};

FrameworkElement around() {
#include "Snippets/Border/BorderAroundTextblock.h"

    return gallery::controlExample({
        .header = gallery::snippet(aroundHeader),
        .example = border,
        .options = {thickness, Grid {columnDefinitions = u"*,*", columnSpacing = 8.0, backgroundChoice, brushChoice}},
        .code = gallery::snippet(aroundCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::borderPage() {
    return StackPanel {around()};
}