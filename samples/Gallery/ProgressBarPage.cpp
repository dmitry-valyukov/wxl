// Страница ProgressBar -- ProgressBarPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t indeterminateHeader[] = {
#include "Snippets/ProgressBar/IndeterminateProgressBar.html.embed"
};
constexpr char8_t indeterminateCode[] = {
#include "Snippets/ProgressBar/IndeterminateProgressBar.h.embed"
};

FrameworkElement indeterminate() {
#include "Snippets/ProgressBar/IndeterminateProgressBar.h"

    return gallery::controlExample({
        .header = gallery::snippet(indeterminateHeader),
        .example = bar,
        .options = {stateGroup},
        .code = gallery::snippet(indeterminateCode),
    });
}

constexpr char8_t determinateHeader[] = {
#include "Snippets/ProgressBar/DeterminateProgressBar.html.embed"
};
constexpr char8_t determinateCode[] = {
#include "Snippets/ProgressBar/DeterminateProgressBar.h.embed"
};

FrameworkElement determinate() {
#include "Snippets/ProgressBar/DeterminateProgressBar.h"

    return gallery::controlExample({
        .header = gallery::snippet(determinateHeader),
        .example = row,
        .code = gallery::snippet(determinateCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::progressBarPage() {
    return StackPanel {indeterminate(), determinate()};
}
