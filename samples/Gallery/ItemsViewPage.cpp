// Страница ItemsView -- ItemsViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "Box.h"
#include "ItemBuilder.h"
#include "CustomDataObject.h"
#include <wxl/Microsoft.UI.Dispatching.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicViewHeader[] = {
#include "Snippets/ItemsView/BasicItemsview.html.embed"
};
constexpr char8_t basicViewCode[] = {
#include "Snippets/ItemsView/BasicItemsview.h.embed"
};

FrameworkElement basicView() {
#include "Snippets/ItemsView/BasicItemsview.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicViewHeader),
        .example = example,
        .code = gallery::snippet(basicViewCode),
    });
}

constexpr char8_t swappableHeader[] = {
#include "Snippets/ItemsView/ItemsviewSwappableLayouts.html.embed"
};
constexpr char8_t swappableCode[] = {
#include "Snippets/ItemsView/ItemsviewSwappableLayouts.h.embed"
};

FrameworkElement swappable() {
#include "Snippets/ItemsView/ItemsviewSwappableLayouts.h"

    return gallery::controlExample({
        .header = gallery::snippet(swappableHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(swappableCode),
    });
}

constexpr char8_t invocationHeader[] = {
#include "Snippets/ItemsView/ItemsviewItemInvocationSelection.html.embed"
};
constexpr char8_t invocationCode[] = {
#include "Snippets/ItemsView/ItemsviewItemInvocationSelection.h.embed"
};

FrameworkElement invocation() {
#include "Snippets/ItemsView/ItemsviewItemInvocationSelection.h"

    return gallery::controlExample({
        .header = gallery::snippet(invocationHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(invocationCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::itemsViewPage() {
    return StackPanel {basicView(), swappable(), invocation()};
}
