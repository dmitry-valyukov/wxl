// Страница Line -- LinePage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t lineHeader[] = {
#include "Snippets/Line/Line.html.embed"
};
constexpr char8_t lineCode[] = {
#include "Snippets/Line/Line.h.embed"
};

FrameworkElement line() {
#include "Snippets/Line/Line.h"

    return gallery::controlExample({
        .header = gallery::snippet(lineHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(lineCode),
    });
}

constexpr char8_t polylineHeader[] = {
#include "Snippets/Line/Polyline.html.embed"
};
constexpr char8_t polylineCode[] = {
#include "Snippets/Line/Polyline.h.embed"
};

FrameworkElement polyline() {
#include "Snippets/Line/Polyline.h"

    return gallery::controlExample({
        .header = gallery::snippet(polylineHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(polylineCode),
    });
}

constexpr char8_t pathHeader[] = {
#include "Snippets/Line/LinePath.html.embed"
};
constexpr char8_t pathCode[] = {
#include "Snippets/Line/LinePath.h.embed"
};

FrameworkElement path() {
#include "Snippets/Line/LinePath.h"

    return gallery::controlExample({
        .header = gallery::snippet(pathHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(pathCode),
    });
}

constexpr char8_t geometryGroupHeader[] = {
#include "Snippets/Line/LineGeometrygroup.html.embed"
};
constexpr char8_t geometryGroupCode[] = {
#include "Snippets/Line/LineGeometrygroup.h.embed"
};

FrameworkElement geometryGroup() {
#include "Snippets/Line/LineGeometrygroup.h"

    return gallery::controlExample({
        .header = gallery::snippet(geometryGroupHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(geometryGroupCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::linePage() {
    return StackPanel {line(), polyline(), path(), geometryGroup()};
}
