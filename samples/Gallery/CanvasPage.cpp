// Страница Canvas — CanvasPage оригинала: один пример.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlHeader[] = {
#include "Snippets/Canvas/CanvasControl.html.embed"
};
constexpr char8_t controlCode[] = {
#include "Snippets/Canvas/CanvasControl.h.embed"
};

FrameworkElement control() {
#include "Snippets/Canvas/CanvasControl.h"

    return gallery::controlExample({
        .header = gallery::snippet(controlHeader),
        .example = canvas,
        .options = {StackPanel {orientation.horizontal, topSlider, StackPanel {Margin {16, 0, 0, 0}, leftSlider, zSlider}}},
        .code = gallery::snippet(controlCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::canvasPage() {
    return StackPanel {control()};
}