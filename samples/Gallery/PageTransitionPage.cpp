// Страница PageTransition -- PageTransitionPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <memory>
#include "MotionPages.h"
#include "PagedFrame.h"
#include "ResourceBrush.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.Animation.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t pageTransitionsHeader[] = {
#include "Snippets/PageTransition/PageTransitions.html.embed"
};
constexpr char8_t pageTransitionsCode[] = {
#include "Snippets/PageTransition/PageTransitions.h.embed"
};

FrameworkElement pageTransitions() {
#include "Snippets/PageTransition/PageTransitions.h"

    return gallery::controlExample({
        .header = gallery::snippet(pageTransitionsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(pageTransitionsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::pageTransitionPage() {
    return StackPanel {pageTransitions()};
}
