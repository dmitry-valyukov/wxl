// Страница IconElement -- IconElementPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t bitmapIconHeader[] = {
#include "Snippets/IconElement/MulticolorBitmap.html.embed"
};
constexpr char8_t bitmapIconCode[] = {
#include "Snippets/IconElement/MulticolorBitmap.h.embed"
};

FrameworkElement bitmapIcon() {
#include "Snippets/IconElement/MulticolorBitmap.h"

    return gallery::controlExample({
        .header = gallery::snippet(bitmapIconHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(bitmapIconCode),
    });
}

constexpr char8_t fontIconHeader[] = {
#include "Snippets/IconElement/FontGlyph.html.embed"
};
constexpr char8_t fontIconCode[] = {
#include "Snippets/IconElement/FontGlyph.h.embed"
};

FrameworkElement fontIcon() {
#include "Snippets/IconElement/FontGlyph.h"

    return gallery::controlExample({
        .header = gallery::snippet(fontIconHeader),
        .example = example,
        .code = gallery::snippet(fontIconCode),
    });
}

constexpr char8_t imageIconHeader[] = {
#include "Snippets/IconElement/ImageBitmap.html.embed"
};
constexpr char8_t imageIconCode[] = {
#include "Snippets/IconElement/ImageBitmap.h.embed"
};

FrameworkElement imageIcon() {
#include "Snippets/IconElement/ImageBitmap.h"

    return gallery::controlExample({
        .header = gallery::snippet(imageIconHeader),
        .example = example,
        .code = gallery::snippet(imageIconCode),
    });
}

constexpr char8_t svgIconHeader[] = {
#include "Snippets/IconElement/ImageSvg.html.embed"
};
constexpr char8_t svgIconCode[] = {
#include "Snippets/IconElement/ImageSvg.h.embed"
};

FrameworkElement svgIcon() {
    return gallery::controlExample({
        .header = gallery::snippet(svgIconHeader),
        .example =
#include "Snippets/IconElement/ImageSvg.h"
        ,
        .code = gallery::snippet(svgIconCode),
    });
}

constexpr char8_t pathIconHeader[] = {
#include "Snippets/IconElement/Path.html.embed"
};
constexpr char8_t pathIconCode[] = {
#include "Snippets/IconElement/Path.h.embed"
};

FrameworkElement pathIcon() {
#include "Snippets/IconElement/Path.h"

    return gallery::controlExample({
        .header = gallery::snippet(pathIconHeader),
        .example = example,
        .code = gallery::snippet(pathIconCode),
    });
}

constexpr char8_t symbolIconHeader[] = {
#include "Snippets/IconElement/Symbol.html.embed"
};
constexpr char8_t symbolIconCode[] = {
#include "Snippets/IconElement/Symbol.h.embed"
};

FrameworkElement symbolIcon() {
#include "Snippets/IconElement/Symbol.h"

    return gallery::controlExample({
        .header = gallery::snippet(symbolIconHeader),
        .example = example,
        .code = gallery::snippet(symbolIconCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::iconElementPage() {
    return StackPanel {bitmapIcon(), fontIcon(), imageIcon(), svgIcon(), pathIcon(), symbolIcon()};
}
