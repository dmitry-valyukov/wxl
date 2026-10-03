// Страница XamlCompInterop -- XamlCompInteropPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <chrono>
#include <memory>
#include <string>
#include <vector>
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Input.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t naturalMotionHeader[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropNaturalMotionCompositionAnimation.html.embed"
};
constexpr char8_t naturalMotionCode[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropNaturalMotionCompositionAnimation.h.embed"
};

FrameworkElement naturalMotion() {
#include "Snippets/XamlCompInterop/XamlCompInteropNaturalMotionCompositionAnimation.h"

    return gallery::controlExample({
        .header = gallery::snippet(naturalMotionHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(naturalMotionCode),
    });
}

constexpr char8_t ellipseHeader[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropExpressionanimationEllipseElement.html.embed"
};
constexpr char8_t ellipseCode[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropExpressionanimationEllipseElement.h.embed"
};

FrameworkElement ellipse() {
#include "Snippets/XamlCompInterop/XamlCompInteropExpressionanimationEllipseElement.h"

    return gallery::controlExample({
        .header = gallery::snippet(ellipseHeader),
        .example = example,
        .code = gallery::snippet(ellipseCode),
    });
}

constexpr char8_t severalHeader[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropDrivingSeveralRelatedAnimations.html.embed"
};
constexpr char8_t severalCode[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropDrivingSeveralRelatedAnimations.h.embed"
};

FrameworkElement several() {
#include "Snippets/XamlCompInterop/XamlCompInteropDrivingSeveralRelatedAnimations.h"

    return gallery::controlExample({
        .header = gallery::snippet(severalHeader),
        .example = example,
        .code = gallery::snippet(severalCode),
    });
}

constexpr char8_t circleHeader[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualsizeExpressionanimationsMake.html.embed"
};
constexpr char8_t circleCode[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualsizeExpressionanimationsMake.h.embed"
};

FrameworkElement circle() {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualsizeExpressionanimationsMake.h"

    return gallery::controlExample({
        .header = gallery::snippet(circleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(circleCode),
    });
}

constexpr char8_t actualOffsetHeader[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualoffsetActualsizeExpressionanimations.html.embed"
};
constexpr char8_t actualOffsetCode[] = {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualoffsetActualsizeExpressionanimations.h.embed"
};

FrameworkElement actualOffset() {
#include "Snippets/XamlCompInterop/XamlCompInteropReferenceActualoffsetActualsizeExpressionanimations.h"

    return gallery::controlExample({
        .header = gallery::snippet(actualOffsetHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(actualOffsetCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::xamlCompInteropPage() {
    return StackPanel {naturalMotion(), ellipse(), several(), circle(), actualOffset()};
}
