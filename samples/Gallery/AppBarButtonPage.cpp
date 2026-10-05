// Страница AppBarButton -- AppBarButtonPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t symbolIconHeader[] = {
#include "Snippets/AppBarButton/SymbolIcon.html.embed"
};
constexpr char8_t symbolIconCode[] = {
#include "Snippets/AppBarButton/SymbolIcon.h.embed"
};

FrameworkElement symbolIcon() {
#include "Snippets/AppBarButton/SymbolIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(symbolIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(symbolIconCode),
    });
}

constexpr char8_t bitmapIconHeader[] = {
#include "Snippets/AppBarButton/BitmapIcon.html.embed"
};
constexpr char8_t bitmapIconCode[] = {
#include "Snippets/AppBarButton/BitmapIcon.h.embed"
};

FrameworkElement bitmapIcon() {
#include "Snippets/AppBarButton/BitmapIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(bitmapIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(bitmapIconCode),
    });
}

constexpr char8_t fontIconHeader[] = {
#include "Snippets/AppBarButton/FontIcon.html.embed"
};
constexpr char8_t fontIconCode[] = {
#include "Snippets/AppBarButton/FontIcon.h.embed"
};

FrameworkElement fontIcon() {
#include "Snippets/AppBarButton/FontIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(fontIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(fontIconCode),
    });
}

constexpr char8_t pathIconHeader[] = {
#include "Snippets/AppBarButton/PathIcon.html.embed"
};
constexpr char8_t pathIconCode[] = {
#include "Snippets/AppBarButton/PathIcon.h.embed"
};

FrameworkElement pathIcon() {
#include "Snippets/AppBarButton/PathIcon.h"

    return gallery::controlExample({
        .header = gallery::snippet(pathIconHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(pathIconCode),
    });
}

constexpr char8_t acceleratorHeader[] = {
#include "Snippets/AppBarButton/KeyboardAccelerator.html.embed"
};
constexpr char8_t acceleratorCode[] = {
#include "Snippets/AppBarButton/KeyboardAccelerator.h.embed"
};

FrameworkElement accelerator() {
#include "Snippets/AppBarButton/KeyboardAccelerator.h"

    return gallery::controlExample({
        .header = gallery::snippet(acceleratorHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(acceleratorCode),
    });
}

constexpr char8_t flyoutButtonHeader[] = {
#include "Snippets/AppBarButton/Flyout.html.embed"
};
constexpr char8_t flyoutButtonCode[] = {
#include "Snippets/AppBarButton/Flyout.h.embed"
};

FrameworkElement flyoutButton() {
#include "Snippets/AppBarButton/Flyout.h"

    return gallery::controlExample({
        .header = gallery::snippet(flyoutButtonHeader),
        .example = StackPanel {orientation.horizontal, button},
        .code = gallery::snippet(flyoutButtonCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::appBarButtonPage() {
    return StackPanel {symbolIcon(), bitmapIcon(), fontIcon(), pathIcon(), accelerator(), flyoutButton()};
}
