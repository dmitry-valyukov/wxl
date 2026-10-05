// Страница SelectorBar -- SelectorBarPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <vector>
#include "Box.h"
#include "NavigationPages.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Media.Animation.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/SelectorBar/BasicSelectorbar.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/SelectorBar/BasicSelectorbar.h.embed"
};

FrameworkElement basic() {
#include "Snippets/SelectorBar/BasicSelectorbar.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t frameTransitionsHeader[] = {
#include "Snippets/SelectorBar/SelectorbarFrameSlideTransitions.html.embed"
};
constexpr char8_t frameTransitionsCode[] = {
#include "Snippets/SelectorBar/SelectorbarFrameSlideTransitions.h.embed"
};

FrameworkElement frameTransitions() {
#include "Snippets/SelectorBar/SelectorbarFrameSlideTransitions.h"

    return gallery::controlExample({
        .header = gallery::snippet(frameTransitionsHeader),
        .example = example,
        .code = gallery::snippet(frameTransitionsCode),
    });
}

constexpr char8_t collectionsHeader[] = {
#include "Snippets/SelectorBar/SelectorbarDisplayingDifferentCollections.html.embed"
};
constexpr char8_t collectionsCode[] = {
#include "Snippets/SelectorBar/SelectorbarDisplayingDifferentCollections.h.embed"
};

FrameworkElement collections() {
#include "Snippets/SelectorBar/SelectorbarDisplayingDifferentCollections.h"

    return gallery::controlExample({
        .header = gallery::snippet(collectionsHeader),
        .example = example,
        .code = gallery::snippet(collectionsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::selectorBarPage() {
    return StackPanel {basic(), frameTransitions(), collections()};
}
