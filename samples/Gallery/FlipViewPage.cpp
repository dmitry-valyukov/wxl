// Страница FlipView -- FlipViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <ranges>
#include "generated/Microsoft.UI.Xaml.Controls.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t declaredHeader[] = {
#include "Snippets/FlipView/SimpleFlipviewItemsDeclared.html.embed"
};
constexpr char8_t declaredCode[] = {
#include "Snippets/FlipView/SimpleFlipviewItemsDeclared.h.embed"
};

FrameworkElement declared() {
#include "Snippets/FlipView/SimpleFlipviewItemsDeclared.h"

    return gallery::controlExample({
        .header = gallery::snippet(declaredHeader),
        .example = example,
        .code = gallery::snippet(declaredCode),
    });
}

constexpr char8_t boundHeader[] = {
#include "Snippets/FlipView/FlipviewShowingBoundData.html.embed"
};
constexpr char8_t boundCode[] = {
#include "Snippets/FlipView/FlipviewShowingBoundData.h.embed"
};

FrameworkElement bound() {
#include "Snippets/FlipView/FlipviewShowingBoundData.h"

    return gallery::controlExample({
        .header = gallery::snippet(boundHeader),
        .example = example,
        .code = gallery::snippet(boundCode),
    });
}

constexpr char8_t verticalHeader[] = {
#include "Snippets/FlipView/VerticalFlipview.html.embed"
};
constexpr char8_t verticalCode[] = {
#include "Snippets/FlipView/VerticalFlipview.h.embed"
};

FrameworkElement vertical() {
#include "Snippets/FlipView/VerticalFlipview.h"

    return gallery::controlExample({
        .header = gallery::snippet(verticalHeader),
        .example = example,
        .code = gallery::snippet(verticalCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::flipViewPage() {
    return StackPanel {declared(), bound(), vertical()};
}
