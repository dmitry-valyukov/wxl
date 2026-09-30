// Страница MenuBar -- MenuBarPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/MenuBar/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/MenuBar/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/MenuBar/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {output, bar},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t acceleratorsHeader[] = {
#include "Snippets/MenuBar/KeyboardAccelerators.html.embed"
};
constexpr char8_t acceleratorsCode[] = {
#include "Snippets/MenuBar/KeyboardAccelerators.h.embed"
};

FrameworkElement accelerators() {
#include "Snippets/MenuBar/KeyboardAccelerators.h"

    return gallery::controlExample({
        .header = gallery::snippet(acceleratorsHeader),
        .example = StackPanel {output, bar},
        .code = gallery::snippet(acceleratorsCode),
    });
}

constexpr char8_t submenusHeader[] = {
#include "Snippets/MenuBar/Submenus.html.embed"
};
constexpr char8_t submenusCode[] = {
#include "Snippets/MenuBar/Submenus.h.embed"
};

FrameworkElement submenus() {
#include "Snippets/MenuBar/Submenus.h"

    return gallery::controlExample({
        .header = gallery::snippet(submenusHeader),
        .example = StackPanel {output, bar},
        .code = gallery::snippet(submenusCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::menuBarPage() {
    return StackPanel {simple(), accelerators(), submenus()};
}
