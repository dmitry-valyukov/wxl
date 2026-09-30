// Страница AppBarToggleButton -- AppBarToggleButtonPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t symbolIconHeader[] = {
#include "Snippets/AppBarToggleButton/SymbolIcon.html.embed"
};
constexpr char8_t symbolIconCode[] = {
#include "Snippets/AppBarToggleButton/SymbolIcon.h.embed"
};

FrameworkElement symbolIcon() {
#include "Snippets/AppBarToggleButton/SymbolIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(symbolIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(symbolIconCode),
    });
}

constexpr char8_t bitmapIconHeader[] = {
#include "Snippets/AppBarToggleButton/BitmapIcon.html.embed"
};
constexpr char8_t bitmapIconCode[] = {
#include "Snippets/AppBarToggleButton/BitmapIcon.h.embed"
};

FrameworkElement bitmapIcon() {
#include "Snippets/AppBarToggleButton/BitmapIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(bitmapIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(bitmapIconCode),
    });
}

constexpr char8_t fontIconHeader[] = {
#include "Snippets/AppBarToggleButton/FontIcon.html.embed"
};
constexpr char8_t fontIconCode[] = {
#include "Snippets/AppBarToggleButton/FontIcon.h.embed"
};

FrameworkElement fontIcon() {
#include "Snippets/AppBarToggleButton/FontIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(fontIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(fontIconCode),
    });
}

constexpr char8_t threeStateHeader[] = {
#include "Snippets/AppBarToggleButton/ThreeState.html.embed"
};
constexpr char8_t threeStateCode[] = {
#include "Snippets/AppBarToggleButton/ThreeState.h.embed"
};

FrameworkElement threeState() {
#include "Snippets/AppBarToggleButton/ThreeState.h"

    return gallery::controlExample({
        .header = gallery::snippet(threeStateHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(threeStateCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::appBarToggleButtonPage() {
    return StackPanel {symbolIcon(), bitmapIcon(), fontIcon(), threeState()};
}
