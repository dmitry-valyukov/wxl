// Страница NavigationView -- NavigationViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <string>
#include <vector>
#include "Box.h"
#include "BoundTemplate.h"
#include "NavigationPages.h"
#include "StringList.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t defaultModeHeader[] = {
#include "Snippets/NavigationView/NavigationviewDefaultPanedisplaymode.html.embed"
};
constexpr char8_t defaultModeCode[] = {
#include "Snippets/NavigationView/NavigationviewDefaultPanedisplaymode.h.embed"
};

FrameworkElement defaultMode() {
#include "Snippets/NavigationView/NavigationviewDefaultPanedisplaymode.h"

    return gallery::controlExample({
        .header = gallery::snippet(defaultModeHeader),
        .example = example,
        .code = gallery::snippet(defaultModeCode),
    });
}

constexpr char8_t topModeHeader[] = {
#include "Snippets/NavigationView/NavigationviewPanedisplaymodeTop.html.embed"
};
constexpr char8_t topModeCode[] = {
#include "Snippets/NavigationView/NavigationviewPanedisplaymodeTop.h.embed"
};

FrameworkElement topMode() {
#include "Snippets/NavigationView/NavigationviewPanedisplaymodeTop.h"

    return gallery::controlExample({
        .header = gallery::snippet(topModeHeader),
        .example = example,
        .code = gallery::snippet(topModeCode),
    });
}

constexpr char8_t switchingModeHeader[] = {
#include "Snippets/NavigationView/NavigationviewSwitchesPaneOrientation.html.embed"
};
constexpr char8_t switchingModeCode[] = {
#include "Snippets/NavigationView/NavigationviewSwitchesPaneOrientation.h.embed"
};

FrameworkElement switchingMode() {
#include "Snippets/NavigationView/NavigationviewSwitchesPaneOrientation.h"

    return gallery::controlExample({
        .header = gallery::snippet(switchingModeHeader),
        .example = example,
        .code = gallery::snippet(switchingModeCode),
    });
}

constexpr char8_t tabsPatternHeader[] = {
#include "Snippets/NavigationView/NavigationViewTyingSelectionFocusTabs.html.embed"
};
constexpr char8_t tabsPatternCode[] = {
#include "Snippets/NavigationView/NavigationViewTyingSelectionFocusTabs.h.embed"
};

FrameworkElement tabsPattern() {
#include "Snippets/NavigationView/NavigationViewTyingSelectionFocusTabs.h"

    return gallery::controlExample({
        .header = gallery::snippet(tabsPatternHeader),
        .example = example,
        .code = gallery::snippet(tabsPatternCode),
    });
}

constexpr char8_t dataBindingHeader[] = {
#include "Snippets/NavigationView/NavigationViewDataBinding.html.embed"
};
constexpr char8_t dataBindingCode[] = {
#include "Snippets/NavigationView/NavigationViewDataBinding.h.embed"
};

FrameworkElement dataBinding() {
#include "Snippets/NavigationView/NavigationViewDataBinding.h"

    return gallery::controlExample({
        .header = gallery::snippet(dataBindingHeader),
        .example = example,
        .code = gallery::snippet(dataBindingCode),
    });
}

constexpr char8_t footerItemsHeader[] = {
#include "Snippets/NavigationView/NavigationviewFooterMenuItems.html.embed"
};
constexpr char8_t footerItemsCode[] = {
#include "Snippets/NavigationView/NavigationviewFooterMenuItems.h.embed"
};

FrameworkElement footerItems() {
#include "Snippets/NavigationView/NavigationviewFooterMenuItems.h"

    return gallery::controlExample({
        .header = gallery::snippet(footerItemsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(footerItemsCode),
    });
}

constexpr char8_t hierarchicalHeader[] = {
#include "Snippets/NavigationView/HierarchicalNavigationview.html.embed"
};
constexpr char8_t hierarchicalCode[] = {
#include "Snippets/NavigationView/HierarchicalNavigationview.h.embed"
};

FrameworkElement hierarchical() {
#include "Snippets/NavigationView/HierarchicalNavigationview.h"

    return gallery::controlExample({
        .header = gallery::snippet(hierarchicalHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(hierarchicalCode),
    });
}

constexpr char8_t apiActionHeader[] = {
#include "Snippets/NavigationView/NavigationViewApiAction.html.embed"
};
constexpr char8_t apiActionCode[] = {
#include "Snippets/NavigationView/NavigationViewApiAction.h.embed"
};

FrameworkElement apiAction() {
#include "Snippets/NavigationView/NavigationViewApiAction.h"

    return gallery::controlExample({
        .header = gallery::snippet(apiActionHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(apiActionCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::navigationViewPage() {
    return StackPanel {defaultMode(), topMode(), switchingMode(), tabsPattern(), dataBinding(), footerItems(), hierarchical(), apiAction()};
}
