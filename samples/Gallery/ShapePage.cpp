// Страница Shape -- ShapePage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t ellipseHeader[] = {
#include "Snippets/Shape/Ellipse.html.embed"
};
constexpr char8_t ellipseCode[] = {
#include "Snippets/Shape/Ellipse.h.embed"
};

FrameworkElement ellipse() {
#include "Snippets/Shape/Ellipse.h"

    return gallery::controlExample({
        .header = gallery::snippet(ellipseHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(ellipseCode),
    });
}

constexpr char8_t rectangleHeader[] = {
#include "Snippets/Shape/Rectangle.html.embed"
};
constexpr char8_t rectangleCode[] = {
#include "Snippets/Shape/Rectangle.h.embed"
};

FrameworkElement rectangle() {
#include "Snippets/Shape/Rectangle.h"

    return gallery::controlExample({
        .header = gallery::snippet(rectangleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(rectangleCode),
    });
}

constexpr char8_t polygonHeader[] = {
#include "Snippets/Shape/Polygon.html.embed"
};
constexpr char8_t polygonCode[] = {
#include "Snippets/Shape/Polygon.h.embed"
};

FrameworkElement polygon() {
#include "Snippets/Shape/Polygon.h"

    return gallery::controlExample({
        .header = gallery::snippet(polygonHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(polygonCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::shapePage() {
    return StackPanel {ellipse(), rectangle(), polygon()};
}
