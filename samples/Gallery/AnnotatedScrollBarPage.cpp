// Страница AnnotatedScrollBar -- AnnotatedScrollBarPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t linkedHeader[] = {
#include "Snippets/AnnotatedScrollBar/LinkedScrollView.html.embed"
};
constexpr char8_t linkedCode[] = {
#include "Snippets/AnnotatedScrollBar/LinkedScrollView.h.embed"
};

FrameworkElement linked() {
#include "Snippets/AnnotatedScrollBar/LinkedScrollView.h"

    return gallery::controlExample({
        .header = gallery::snippet(linkedHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(linkedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::annotatedScrollBarPage() {
    return StackPanel {linked()};
}
