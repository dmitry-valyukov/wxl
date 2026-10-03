// Страница ThemeTransition -- ThemeTransitionPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <memory>
#include <string>
#include <vector>
#include "Box.h"
#include "RepeaterData.h"
#include "StringList.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.Animation.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"
#include "generated/Microsoft.UI.Xaml.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t entranceExampleHeader[] = {
#include "Snippets/ThemeTransition/EntrancethemetransitionAddingItemsYour.html.embed"
};
constexpr char8_t entranceExampleCode[] = {
#include "Snippets/ThemeTransition/EntrancethemetransitionAddingItemsYour.h.embed"
};

FrameworkElement entranceExample() {
#include "Snippets/ThemeTransition/EntrancethemetransitionAddingItemsYour.h"

    return gallery::controlExample({
        .header = gallery::snippet(entranceExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(entranceExampleCode),
    });
}

constexpr char8_t repositionExampleHeader[] = {
#include "Snippets/ThemeTransition/RepositionthemetransitionReactLayoutChanges.html.embed"
};
constexpr char8_t repositionExampleCode[] = {
#include "Snippets/ThemeTransition/RepositionthemetransitionReactLayoutChanges.h.embed"
};

FrameworkElement repositionExample() {
#include "Snippets/ThemeTransition/RepositionthemetransitionReactLayoutChanges.h"

    return gallery::controlExample({
        .header = gallery::snippet(repositionExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(repositionExampleCode),
    });
}

constexpr char8_t contentExampleHeader[] = {
#include "Snippets/ThemeTransition/ContentthemetransitionAnimateContentRefreshes.html.embed"
};
constexpr char8_t contentExampleCode[] = {
#include "Snippets/ThemeTransition/ContentthemetransitionAnimateContentRefreshes.h.embed"
};

FrameworkElement contentExample() {
#include "Snippets/ThemeTransition/ContentthemetransitionAnimateContentRefreshes.h"

    return gallery::controlExample({
        .header = gallery::snippet(contentExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(contentExampleCode),
    });
}

constexpr char8_t addDeleteExampleHeader[] = {
#include "Snippets/ThemeTransition/AdddeletethemetransitionAnimateAddingRemoving.html.embed"
};
constexpr char8_t addDeleteExampleCode[] = {
#include "Snippets/ThemeTransition/AdddeletethemetransitionAnimateAddingRemoving.h.embed"
};

FrameworkElement addDeleteExample() {
#include "Snippets/ThemeTransition/AdddeletethemetransitionAnimateAddingRemoving.h"

    return gallery::controlExample({
        .header = gallery::snippet(addDeleteExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(addDeleteExampleCode),
    });
}

constexpr char8_t popupExampleHeader[] = {
#include "Snippets/ThemeTransition/PopupthemetransitionAnimateOpeningClosing.html.embed"
};
constexpr char8_t popupExampleCode[] = {
#include "Snippets/ThemeTransition/PopupthemetransitionAnimateOpeningClosing.h.embed"
};

FrameworkElement popupExample() {
#include "Snippets/ThemeTransition/PopupthemetransitionAnimateOpeningClosing.h"

    return gallery::controlExample({
        .header = gallery::snippet(popupExampleHeader),
        .example = example,
        .code = gallery::snippet(popupExampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::themeTransitionPage() {
    return StackPanel {entranceExample(), repositionExample(), contentExample(), addDeleteExample(), popupExample()};
}
