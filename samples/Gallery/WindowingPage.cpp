// Страница Windowing -- WindowingPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t createWindowHeader[] = {
#include "Snippets/Windowing/CreateMultipleWindows.html.embed"
};
constexpr char8_t createWindowCode[] = {
#include "Snippets/Windowing/CreateMultipleWindows.h.embed"
};

FrameworkElement createWindow() {
#include "Snippets/Windowing/CreateMultipleWindows.h"

    return gallery::controlExample({
        .header = gallery::snippet(createWindowHeader),
        .example = example,
        .code = gallery::snippet(createWindowCode),
    });
}

constexpr char8_t sizingHeader[] = {
#include "Snippets/Windowing/WindowSizing.html.embed"
};
constexpr char8_t sizingCode[] = {
#include "Snippets/Windowing/WindowSizing.h.embed"
};

FrameworkElement sizing() {
#include "Snippets/Windowing/WindowSizing.h"

    return gallery::controlExample({
        .header = gallery::snippet(sizingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(sizingCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::windowingPage() {
    return StackPanel {
        TextBlock {
            Margin {0, 0, 0, 8},
            textWrapping = TextWrapping::Wrap,
            u"Create top-level XAML windows and configure their client-area size. Each example opens a separate window.",
        },createWindow(), sizing()};
}
