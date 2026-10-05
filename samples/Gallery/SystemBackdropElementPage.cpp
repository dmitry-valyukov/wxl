// Страница SystemBackdropElement -- SystemBackdropElementPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t sampleHeader[] = {
#include "Snippets/SystemBackdropElement/SystembackdropelementSample.html.embed"
};
constexpr char8_t sampleCode[] = {
#include "Snippets/SystemBackdropElement/SystembackdropelementSample.h.embed"
};

FrameworkElement sample() {
#include "Snippets/SystemBackdropElement/SystembackdropelementSample.h"

    return gallery::controlExample({
        .header = gallery::snippet(sampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(sampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::systemBackdropElementPage() {
    return StackPanel {
        sample(),
        TextBlock {
            Margin {0, 12, 0, 0},
            textWrapping = TextWrapping::WrapWholeWords,
            u"Theme updates for SystemBackdropElement are triggered by app or OS theme changes, not by setting theme "
            u"directly on the element or a parent control.",
        },
    };
}
