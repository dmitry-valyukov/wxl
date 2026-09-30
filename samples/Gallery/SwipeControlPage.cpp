// Страница SwipeControl -- SwipeControlPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t revealActionsHeader[] = {
#include "Snippets/SwipeControl/RevealActions.html.embed"
};
constexpr char8_t revealActionsCode[] = {
#include "Snippets/SwipeControl/RevealActions.h.embed"
};

FrameworkElement revealActions() {
#include "Snippets/SwipeControl/RevealActions.h"

    return gallery::controlExample({
        .header = gallery::snippet(revealActionsHeader),
        .example = swipe,
        .code = gallery::snippet(revealActionsCode),
    });
}

constexpr char8_t invokeExecuteHeader[] = {
#include "Snippets/SwipeControl/InvokeExecute.html.embed"
};
constexpr char8_t invokeExecuteCode[] = {
#include "Snippets/SwipeControl/InvokeExecute.h.embed"
};

FrameworkElement invokeExecute() {
#include "Snippets/SwipeControl/InvokeExecute.h"

    return gallery::controlExample({
        .header = gallery::snippet(invokeExecuteHeader),
        .example = swipe,
        .code = gallery::snippet(invokeExecuteCode),
    });
}

constexpr char8_t listViewHeader[] = {
#include "Snippets/SwipeControl/ListView.html.embed"
};
constexpr char8_t listViewCode[] = {
#include "Snippets/SwipeControl/ListView.h.embed"
};

FrameworkElement listView() {
#include "Snippets/SwipeControl/ListView.h"

    return gallery::controlExample({
        .header = gallery::snippet(listViewHeader),
        .example = list,
        .code = gallery::snippet(listViewCode),
    });
}

constexpr char8_t gradientHeader[] = {
#include "Snippets/SwipeControl/GradientBackground.html.embed"
};
constexpr char8_t gradientCode[] = {
#include "Snippets/SwipeControl/GradientBackground.h.embed"
};

FrameworkElement gradient() {
#include "Snippets/SwipeControl/GradientBackground.h"

    return gallery::controlExample({
        .header = gallery::snippet(gradientHeader),
        .example = swipe,
        .code = gallery::snippet(gradientCode),
    });
}

constexpr char8_t customIconsHeader[] = {
#include "Snippets/SwipeControl/CustomIcons.html.embed"
};
constexpr char8_t customIconsCode[] = {
#include "Snippets/SwipeControl/CustomIcons.h.embed"
};

FrameworkElement customIcons() {
#include "Snippets/SwipeControl/CustomIcons.h"

    return gallery::controlExample({
        .header = gallery::snippet(customIconsHeader),
        .example = swipe,
        .code = gallery::snippet(customIconsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::swipeControlPage() {
    return StackPanel {revealActions(), invokeExecute(), listView(), gradient(), customIcons()};
}
