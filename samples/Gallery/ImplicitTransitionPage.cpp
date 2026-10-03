// Страница ImplicitTransition -- ImplicitTransitionPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <cmath>
#include <memory>
#include "MotionPages.h"
#include "ResourceBrush.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Input.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"
#include "generated/Microsoft.UI.Xaml.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t opacityExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesOpacity.html.embed"
};
constexpr char8_t opacityExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesOpacity.h.embed"
};

FrameworkElement opacityExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesOpacity.h"

    return gallery::controlExample({
        .header = gallery::snippet(opacityExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(opacityExampleCode),
    });
}

constexpr char8_t rotationExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesRotation.html.embed"
};
constexpr char8_t rotationExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesRotation.h.embed"
};

FrameworkElement rotationExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesRotation.h"

    return gallery::controlExample({
        .header = gallery::snippet(rotationExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(rotationExampleCode),
    });
}

constexpr char8_t scaleExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesScale.html.embed"
};
constexpr char8_t scaleExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesScale.h.embed"
};

FrameworkElement scaleExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesScale.h"

    return gallery::controlExample({
        .header = gallery::snippet(scaleExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(scaleExampleCode),
    });
}

constexpr char8_t translationExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesTranslation.html.embed"
};
constexpr char8_t translationExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesTranslation.h.embed"
};

FrameworkElement translationExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionAutomaticallyAnimateChangesTranslation.h"

    return gallery::controlExample({
        .header = gallery::snippet(translationExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(translationExampleCode),
    });
}

constexpr char8_t backgroundExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateBackgroundChanges.html.embed"
};
constexpr char8_t backgroundExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateBackgroundChanges.h.embed"
};

FrameworkElement backgroundExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateBackgroundChanges.h"

    return gallery::controlExample({
        .header = gallery::snippet(backgroundExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(backgroundExampleCode),
    });
}

constexpr char8_t themeExampleHeader[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateGridTheme.html.embed"
};
constexpr char8_t themeExampleCode[] = {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateGridTheme.h.embed"
};

FrameworkElement themeExample() {
#include "Snippets/ImplicitTransition/ImplicitTransitionImplicitlyAnimateGridTheme.h"

    return gallery::controlExample({
        .header = gallery::snippet(themeExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(themeExampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::implicitTransitionPage() {
    return StackPanel {opacityExample(), rotationExample(), scaleExample(), translationExample(), backgroundExample(), themeExample()};
}
