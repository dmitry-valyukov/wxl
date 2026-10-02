// Страница GridView -- GridViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <ranges>
#include "Box.h"
#include "ItemBuilder.h"
#include "CustomDataObject.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/GridView/BasicGridviewSimpleDatatemplate.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/GridView/BasicGridviewSimpleDatatemplate.h.embed"
};

FrameworkElement basic() {
#include "Snippets/GridView/BasicGridviewSimpleDatatemplate.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t layoutHeader[] = {
#include "Snippets/GridView/GridviewLayoutCustomization.html.embed"
};
constexpr char8_t layoutCode[] = {
#include "Snippets/GridView/GridviewLayoutCustomization.h.embed"
};

FrameworkElement customizedLayout() {
#include "Snippets/GridView/GridviewLayoutCustomization.h"

    return gallery::controlExample({
        .header = gallery::snippet(layoutHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(layoutCode),
    });
}

constexpr char8_t contentHeader[] = {
#include "Snippets/GridView/ContentInsideGridview.html.embed"
};
constexpr char8_t contentCode[] = {
#include "Snippets/GridView/ContentInsideGridview.h.embed"
};

FrameworkElement contentGrid() {
#include "Snippets/GridView/ContentInsideGridview.h"

    return gallery::controlExample({
        .header = gallery::snippet(contentHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(contentCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::gridViewPage() {
    return StackPanel {basic(), customizedLayout(), contentGrid()};
}
