// Страница InfoBadge -- InfoBadgePage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t embeddedHeader[] = {
#include "Snippets/InfoBadge/EmbeddedNavigationView.html.embed"
};
constexpr char8_t embeddedCode[] = {
#include "Snippets/InfoBadge/EmbeddedNavigationView.h.embed"
};

FrameworkElement embedded() {
#include "Snippets/InfoBadge/EmbeddedNavigationView.h"

    return gallery::controlExample({
        .header = gallery::snippet(embeddedHeader),
        .example = Grid {navigation},
        .options = {StackPanel {width = 160, opacity, displayMode}},
        .code = gallery::snippet(embeddedCode),
    });
}

constexpr char8_t differentHeader[] = {
#include "Snippets/InfoBadge/DifferentInfoBadgeStyles.html.embed"
};
constexpr char8_t differentCode[] = {
#include "Snippets/InfoBadge/DifferentInfoBadgeStyles.h.embed"
};

FrameworkElement different() {
#include "Snippets/InfoBadge/DifferentInfoBadgeStyles.h"

    return gallery::controlExample({
        .header = gallery::snippet(differentHeader),
        .example = StackPanel {hAlign.center, spacing = 20.0, orientation.horizontal, iconBadge, valueBadge, dotBadge},
        .options = {StackPanel {width = 160, kind}},
        .code = gallery::snippet(differentCode),
    });
}

constexpr char8_t insideHeader[] = {
#include "Snippets/InfoBadge/PlacingInfoBadgeInsideAnother.html.embed"
};
constexpr char8_t insideCode[] = {
#include "Snippets/InfoBadge/PlacingInfoBadgeInsideAnother.h.embed"
};

FrameworkElement inside() {
    return gallery::controlExample({
        .header = gallery::snippet(insideHeader),
        .example =
#include "Snippets/InfoBadge/PlacingInfoBadgeInsideAnother.h"
        ,
        .code = gallery::snippet(insideCode),
    });
}

constexpr char8_t dynamicHeader[] = {
#include "Snippets/InfoBadge/InfoBadgeDynamicValue.html.embed"
};
constexpr char8_t dynamicCode[] = {
#include "Snippets/InfoBadge/InfoBadgeDynamicValue.h.embed"
};

FrameworkElement dynamic() {
#include "Snippets/InfoBadge/InfoBadgeDynamicValue.h"

    return gallery::controlExample({
        .header = gallery::snippet(dynamicHeader),
        .example = badge,
        .options = {StackPanel {width = 160, valueBox}},
        .code = gallery::snippet(dynamicCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::infoBadgePage() {
    return StackPanel {embedded(), different(), inside(), dynamic()};
}
