// Страница ScrollViewer -- ScrollViewerPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t withImageHeader[] = {
#include "Snippets/ScrollViewer/Content.html.embed"
};
constexpr char8_t withImageCode[] = {
#include "Snippets/ScrollViewer/Content.h.embed"
};

FrameworkElement withImage() {
#include "Snippets/ScrollViewer/Content.h"

    return gallery::controlExample({
        .header = gallery::snippet(withImageHeader),
        .example = viewer,
        .options = {options},
        .code = gallery::snippet(withImageCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::scrollViewerPage() {
    return StackPanel {withImage()};
}
