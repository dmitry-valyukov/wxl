// Страница StackPanel — StackPanelPage оригинала: один пример.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlHeader[] = {
#include "Snippets/StackPanel/StackPanelControl.html.embed"
};
constexpr char8_t controlCode[] = {
#include "Snippets/StackPanel/StackPanelControl.h.embed"
};

FrameworkElement control() {
#include "Snippets/StackPanel/StackPanelControl.h"

    return gallery::controlExample({
        .header = gallery::snippet(controlHeader),
        .example = panel,
        .options = {StackPanel {spacing = 12.0, orientationGroup, gapSlider}},
        .code = gallery::snippet(controlCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::stackPanelPage() {
    return StackPanel {control()};
}