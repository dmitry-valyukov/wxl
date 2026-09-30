// Страница MenuFlyout -- MenuFlyoutPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t appBarButtonMenuHeader[] = {
#include "Snippets/MenuFlyout/AppBarButtonMenu.html.embed"
};
constexpr char8_t appBarButtonMenuCode[] = {
#include "Snippets/MenuFlyout/AppBarButtonMenu.h.embed"
};

FrameworkElement appBarButtonMenu() {
#include "Snippets/MenuFlyout/AppBarButtonMenu.h"

    return gallery::controlExample({
        .header = gallery::snippet(appBarButtonMenuHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(appBarButtonMenuCode),
    });
}

constexpr char8_t togglesHeader[] = {
#include "Snippets/MenuFlyout/Toggles.html.embed"
};
constexpr char8_t togglesCode[] = {
#include "Snippets/MenuFlyout/Toggles.h.embed"
};

FrameworkElement toggles() {
#include "Snippets/MenuFlyout/Toggles.h"

    return gallery::controlExample({
        .header = gallery::snippet(togglesHeader),
        .example = button,
        .code = gallery::snippet(togglesCode),
    });
}

constexpr char8_t cascadingHeader[] = {
#include "Snippets/MenuFlyout/Cascading.html.embed"
};
constexpr char8_t cascadingCode[] = {
#include "Snippets/MenuFlyout/Cascading.h.embed"
};

FrameworkElement cascading() {
#include "Snippets/MenuFlyout/Cascading.h"

    return gallery::controlExample({
        .header = gallery::snippet(cascadingHeader),
        .example = button,
        .code = gallery::snippet(cascadingCode),
    });
}

constexpr char8_t splitHeader[] = {
#include "Snippets/MenuFlyout/Split.html.embed"
};
constexpr char8_t splitCode[] = {
#include "Snippets/MenuFlyout/Split.h.embed"
};

FrameworkElement split() {
#include "Snippets/MenuFlyout/Split.h"

    return gallery::controlExample({
        .header = gallery::snippet(splitHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .code = gallery::snippet(splitCode),
    });
}

constexpr char8_t iconsHeader[] = {
#include "Snippets/MenuFlyout/Icons.html.embed"
};
constexpr char8_t iconsCode[] = {
#include "Snippets/MenuFlyout/Icons.h.embed"
};

FrameworkElement icons() {
#include "Snippets/MenuFlyout/Icons.h"

    return gallery::controlExample({
        .header = gallery::snippet(iconsHeader),
        .example = button,
        .code = gallery::snippet(iconsCode),
    });
}

constexpr char8_t iconsAcceleratorsHeader[] = {
#include "Snippets/MenuFlyout/IconsAccelerators.html.embed"
};
constexpr char8_t iconsAcceleratorsCode[] = {
#include "Snippets/MenuFlyout/IconsAccelerators.h.embed"
};

FrameworkElement iconsAccelerators() {
#include "Snippets/MenuFlyout/IconsAccelerators.h"

    return gallery::controlExample({
        .header = gallery::snippet(iconsAcceleratorsHeader),
        .example = button,
        .code = gallery::snippet(iconsAcceleratorsCode),
    });
}

constexpr char8_t radioHeader[] = {
#include "Snippets/MenuFlyout/Radio.html.embed"
};
constexpr char8_t radioCode[] = {
#include "Snippets/MenuFlyout/Radio.h.embed"
};

FrameworkElement radio() {
#include "Snippets/MenuFlyout/Radio.h"

    return gallery::controlExample({
        .header = gallery::snippet(radioHeader),
        .example = button,
        .code = gallery::snippet(radioCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::menuFlyoutPage() {
    return StackPanel {appBarButtonMenu(), toggles(), cascading(), split(), icons(), iconsAccelerators(), radio()};
}
