// Страница ContentDialog -- ContentDialogPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/ContentDialog/BasicContentDialogContent.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/ContentDialog/BasicContentDialogContent.h.embed"
};

FrameworkElement basic() {
#include "Snippets/ContentDialog/BasicContentDialogContent.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t withoutDefaultHeader[] = {
#include "Snippets/ContentDialog/ContentDialogWithoutDefault.html.embed"
};
constexpr char8_t withoutDefaultCode[] = {
#include "Snippets/ContentDialog/ContentDialogWithoutDefault.h.embed"
};

FrameworkElement withoutDefault() {
#include "Snippets/ContentDialog/ContentDialogWithoutDefault.h"

    return gallery::controlExample({
        .header = gallery::snippet(withoutDefaultHeader),
        .example = example,
        .code = gallery::snippet(withoutDefaultCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::contentDialogPage() {
    return StackPanel {basic(), withoutDefault()};
}
