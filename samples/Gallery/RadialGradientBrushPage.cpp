// Страница RadialGradientBrush -- RadialGradientBrushPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t sampleHeader[] = {
#include "Snippets/RadialGradientBrush/RadialGradientBrushSample.html.embed"
};
constexpr char8_t sampleCode[] = {
#include "Snippets/RadialGradientBrush/RadialGradientBrushSample.h.embed"
};

FrameworkElement sample() {
#include "Snippets/RadialGradientBrush/RadialGradientBrushSample.h"

    return gallery::controlExample({
        .header = gallery::snippet(sampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(sampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::radialGradientBrushPage() {
    return StackPanel {sample()};
}
