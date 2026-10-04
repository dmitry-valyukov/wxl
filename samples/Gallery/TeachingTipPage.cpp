// Страница TeachingTip -- TeachingTipPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t targetedHeader[] = {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipButton.html.embed"
};
constexpr char8_t targetedCode[] = {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipButton.h.embed"
};

FrameworkElement targeted() {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipButton.h"

    return gallery::controlExample({
        .header = gallery::snippet(targetedHeader),
        .example = example,
        .code = gallery::snippet(targetedCode),
    });
}

constexpr char8_t nonTargetedHeader[] = {
#include "Snippets/TeachingTip/ShowNonTargetedTeachingtip.html.embed"
};
constexpr char8_t nonTargetedCode[] = {
#include "Snippets/TeachingTip/ShowNonTargetedTeachingtip.h.embed"
};

FrameworkElement nonTargeted() {
#include "Snippets/TeachingTip/ShowNonTargetedTeachingtip.h"

    return gallery::controlExample({
        .header = gallery::snippet(nonTargetedHeader),
        .example = example,
        .code = gallery::snippet(nonTargetedCode),
    });
}

constexpr char8_t heroHeader[] = {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipHero.html.embed"
};
constexpr char8_t heroCode[] = {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipHero.h.embed"
};

FrameworkElement hero() {
#include "Snippets/TeachingTip/ShowTargetedTeachingtipHero.h"

    return gallery::controlExample({
        .header = gallery::snippet(heroHeader),
        .example = example,
        .code = gallery::snippet(heroCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::teachingTipPage() {
    return StackPanel {targeted(), nonTargeted(), hero()};
}
