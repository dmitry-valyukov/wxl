// Страница ParallaxView -- ParallaxViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "MotionPages.h"
#include "StringList.h"
#include "ResourceBrush.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Shapes.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t listViewHeader[] = {
#include "Snippets/ParallaxView/ParallaxViewParallaxListview.html.embed"
};
constexpr char8_t listViewCode[] = {
#include "Snippets/ParallaxView/ParallaxViewParallaxListview.h.embed"
};

FrameworkElement listView() {
#include "Snippets/ParallaxView/ParallaxViewParallaxListview.h"

    return gallery::controlExample({
        .header = gallery::snippet(listViewHeader),
        .example = example,
        .code = gallery::snippet(listViewCode),
    });
}

constexpr char8_t scrollViewHeader[] = {
#include "Snippets/ParallaxView/ParallaxViewParallaxScrollview.html.embed"
};
constexpr char8_t scrollViewCode[] = {
#include "Snippets/ParallaxView/ParallaxViewParallaxScrollview.h.embed"
};

FrameworkElement scrollView() {
#include "Snippets/ParallaxView/ParallaxViewParallaxScrollview.h"

    return gallery::controlExample({
        .header = gallery::snippet(scrollViewHeader),
        .example = example,
        .code = gallery::snippet(scrollViewCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::parallaxViewPage() {
    return StackPanel {listView(), scrollView()};
}
