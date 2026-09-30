// Страница ToggleButton — ToggleButtonPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/ToggleButton/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/ToggleButton/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/ToggleButton/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {vAlign.top, orientation.horizontal, toggle},
        .output = {output},
        .options = {disable},
        .code = gallery::snippet(simpleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::toggleButtonPage() {
    return StackPanel {simple()};
}