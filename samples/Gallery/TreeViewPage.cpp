// Страница TreeView -- TreeViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "StringList.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t dragDropHeader[] = {
#include "Snippets/TreeView/SimpleTreeviewDragDrop.html.embed"
};
constexpr char8_t dragDropCode[] = {
#include "Snippets/TreeView/SimpleTreeviewDragDrop.h.embed"
};

FrameworkElement dragDrop() {
#include "Snippets/TreeView/SimpleTreeviewDragDrop.h"

    return gallery::controlExample({
        .header = gallery::snippet(dragDropHeader),
        .example = example,
        .code = gallery::snippet(dragDropCode),
    });
}

constexpr char8_t multipleHeader[] = {
#include "Snippets/TreeView/TreeviewMultiSelectionEnabled.html.embed"
};
constexpr char8_t multipleCode[] = {
#include "Snippets/TreeView/TreeviewMultiSelectionEnabled.h.embed"
};

FrameworkElement multiple() {
#include "Snippets/TreeView/TreeviewMultiSelectionEnabled.h"

    return gallery::controlExample({
        .header = gallery::snippet(multipleHeader),
        .example = example,
        .code = gallery::snippet(multipleCode),
    });
}

constexpr char8_t boundHeader[] = {
#include "Snippets/TreeView/TreeviewDatabindingItemsource.html.embed"
};
constexpr char8_t boundCode[] = {
#include "Snippets/TreeView/TreeviewDatabindingItemsource.h.embed"
};

FrameworkElement bound() {
#include "Snippets/TreeView/TreeviewDatabindingItemsource.h"

    return gallery::controlExample({
        .header = gallery::snippet(boundHeader),
        .example = example,
        .code = gallery::snippet(boundCode),
    });
}

constexpr char8_t selectorHeader[] = {
#include "Snippets/TreeView/TreeviewItemtemplateselector.html.embed"
};
constexpr char8_t selectorCode[] = {
#include "Snippets/TreeView/TreeviewItemtemplateselector.h.embed"
};

FrameworkElement selector() {
#include "Snippets/TreeView/TreeviewItemtemplateselector.h"

    return gallery::controlExample({
        .header = gallery::snippet(selectorHeader),
        .example = example,
        .code = gallery::snippet(selectorCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::treeViewPage() {
    return StackPanel {dragDrop(), multiple(), bound(), selector()};
}
