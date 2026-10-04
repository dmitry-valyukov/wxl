// Страница ScrollView -- ScrollViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Windows.Globalization.NumberFormatting.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t withContentHeader[] = {
#include "Snippets/ScrollView/Content.html.embed"
};
constexpr char8_t withContentCode[] = {
#include "Snippets/ScrollView/Content.h.embed"
};

FrameworkElement withContent() {
#include "Snippets/ScrollView/Content.h"

    return gallery::controlExample({
        .header = gallery::snippet(withContentHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(withContentCode),
    });
}

constexpr char8_t constantVelocityHeader[] = {
#include "Snippets/ScrollView/ConstantVelocity.html.embed"
};
constexpr char8_t constantVelocityCode[] = {
#include "Snippets/ScrollView/ConstantVelocity.h.embed"
};

FrameworkElement constantVelocity() {
#include "Snippets/ScrollView/ConstantVelocity.h"

    return gallery::controlExample({
        .header = gallery::snippet(constantVelocityHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(constantVelocityCode),
    });
}

constexpr char8_t customAnimationHeader[] = {
#include "Snippets/ScrollView/CustomAnimation.html.embed"
};
constexpr char8_t customAnimationCode[] = {
#include "Snippets/ScrollView/CustomAnimation.h.embed"
};

FrameworkElement customAnimation() {
#include "Snippets/ScrollView/CustomAnimation.h"

    return gallery::controlExample({
        .header = gallery::snippet(customAnimationHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(customAnimationCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::scrollViewPage() {
    return StackPanel {withContent(), constantVelocity(), customAnimation()};
}
