// Страница EasingFunction -- EasingFunctionPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <chrono>
#include <memory>
#include <vector>
#include "ResourceBrush.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "generated/Microsoft.UI.Xaml.Media.Animation.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t standardHeader[] = {
#include "Snippets/EasingFunction/StandardEasingFunction.html.embed"
};
constexpr char8_t standardCode[] = {
#include "Snippets/EasingFunction/StandardEasingFunction.h.embed"
};

FrameworkElement standard() {
#include "Snippets/EasingFunction/StandardEasingFunction.h"

    return gallery::controlExample({
        .header = gallery::snippet(standardHeader),
        .example = example,
        .code = gallery::snippet(standardCode),
    });
}

constexpr char8_t accelerateHeader[] = {
#include "Snippets/EasingFunction/AccelerateEasingFunction.html.embed"
};
constexpr char8_t accelerateCode[] = {
#include "Snippets/EasingFunction/AccelerateEasingFunction.h.embed"
};

FrameworkElement accelerate() {
#include "Snippets/EasingFunction/AccelerateEasingFunction.h"

    return gallery::controlExample({
        .header = gallery::snippet(accelerateHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(accelerateCode),
    });
}

constexpr char8_t decelerateHeader[] = {
#include "Snippets/EasingFunction/DecelerateEasingFunction.html.embed"
};
constexpr char8_t decelerateCode[] = {
#include "Snippets/EasingFunction/DecelerateEasingFunction.h.embed"
};

FrameworkElement decelerate() {
#include "Snippets/EasingFunction/DecelerateEasingFunction.h"

    return gallery::controlExample({
        .header = gallery::snippet(decelerateHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(decelerateCode),
    });
}

constexpr char8_t otherHeader[] = {
#include "Snippets/EasingFunction/OtherXamlEasingFunctions.html.embed"
};
constexpr char8_t otherCode[] = {
#include "Snippets/EasingFunction/OtherXamlEasingFunctions.h.embed"
};

FrameworkElement other() {
#include "Snippets/EasingFunction/OtherXamlEasingFunctions.h"

    return gallery::controlExample({
        .header = gallery::snippet(otherHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(otherCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::easingFunctionPage() {
    return StackPanel {RichTextBlock {Paragraph {Run {u"- Use the Standard easing function for animating general property changes."}}, Paragraph {Run {u"- Use the Accelerate easing function to animate objects that are exiting the scene."}}, Paragraph {Run {u"- Use the Decelerate easing function to animate objects that are entering the scene."}}}, standard(), accelerate(), decelerate(), other()};
}
