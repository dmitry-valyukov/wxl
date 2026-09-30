// Страница ProgressRing -- ProgressRingPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t indeterminateHeader[] = {
#include "Snippets/ProgressRing/IndeterminateProgressRing.html.embed"
};
constexpr char8_t indeterminateCode[] = {
#include "Snippets/ProgressRing/IndeterminateProgressRing.h.embed"
};

FrameworkElement indeterminate() {
#include "Snippets/ProgressRing/IndeterminateProgressRing.h"

    return gallery::controlExample({
        .header = gallery::snippet(indeterminateHeader),
        .example = ring,
        .options = {activeSwitch},
        .code = gallery::snippet(indeterminateCode),
    });
}

constexpr char8_t determinateHeader[] = {
#include "Snippets/ProgressRing/DeterminateProgressRing.html.embed"
};
constexpr char8_t determinateCode[] = {
#include "Snippets/ProgressRing/DeterminateProgressRing.h.embed"
};

FrameworkElement determinate() {
#include "Snippets/ProgressRing/DeterminateProgressRing.h"

    return gallery::controlExample({
        .header = gallery::snippet(determinateHeader),
        .example = ring,
        .options = {progressSlider},
        .code = gallery::snippet(determinateCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::progressRingPage() {
    return StackPanel {indeterminate(), determinate()};
}
