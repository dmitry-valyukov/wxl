// Страница ListView -- ListViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <cwctype>
#include "Box.h"
#include "ItemBuilder.h"
#include "Contact.h"
#include "generated/Windows.ApplicationModel.DataTransfer.h"
#include "CustomDataObject.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicListHeader[] = {
#include "Snippets/ListView/BasicListviewSimpleDatatemplate.html.embed"
};
constexpr char8_t basicListCode[] = {
#include "Snippets/ListView/BasicListviewSimpleDatatemplate.h.embed"
};

FrameworkElement basicList() {
#include "Snippets/ListView/BasicListviewSimpleDatatemplate.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicListHeader),
        .example = example,
        .code = gallery::snippet(basicListCode),
    });
}

constexpr char8_t selectionHeader[] = {
#include "Snippets/ListView/ListviewSelectionSupport.html.embed"
};
constexpr char8_t selectionCode[] = {
#include "Snippets/ListView/ListviewSelectionSupport.h.embed"
};

FrameworkElement selection() {
#include "Snippets/ListView/ListviewSelectionSupport.h"

    return gallery::controlExample({
        .header = gallery::snippet(selectionHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(selectionCode),
    });
}

constexpr char8_t imagesHeader[] = {
#include "Snippets/ListView/ListviewImages.html.embed"
};
constexpr char8_t imagesCode[] = {
#include "Snippets/ListView/ListviewImages.h.embed"
};

FrameworkElement images() {
#include "Snippets/ListView/ListviewImages.h"

    return gallery::controlExample({
        .header = gallery::snippet(imagesHeader),
        .example = example,
        .code = gallery::snippet(imagesCode),
    });
}

constexpr char8_t filteringHeader[] = {
#include "Snippets/ListView/ListviewFiltering.html.embed"
};
constexpr char8_t filteringCode[] = {
#include "Snippets/ListView/ListviewFiltering.h.embed"
};

FrameworkElement filtering() {
#include "Snippets/ListView/ListviewFiltering.h"

    return gallery::controlExample({
        .header = gallery::snippet(filteringHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(filteringCode),
    });
}

constexpr char8_t menusHeader[] = {
#include "Snippets/ListView/ListviewContextMenus.html.embed"
};
constexpr char8_t menusCode[] = {
#include "Snippets/ListView/ListviewContextMenus.h.embed"
};

FrameworkElement menus() {
#include "Snippets/ListView/ListviewContextMenus.h"

    return gallery::controlExample({
        .header = gallery::snippet(menusHeader),
        .example = example,
        .code = gallery::snippet(menusCode),
    });
}

constexpr char8_t messagingHeader[] = {
#include "Snippets/ListView/ListviewMessagingDataLogging.html.embed"
};
constexpr char8_t messagingCode[] = {
#include "Snippets/ListView/ListviewMessagingDataLogging.h.embed"
};

FrameworkElement messaging() {
#include "Snippets/ListView/ListviewMessagingDataLogging.h"

    return gallery::controlExample({
        .header = gallery::snippet(messagingHeader),
        .example = example,
        .code = gallery::snippet(messagingCode),
    });
}

constexpr char8_t restoringHeader[] = {
#include "Snippets/ListView/ListviewRestoreScrollPosition.html.embed"
};
constexpr char8_t restoringCode[] = {
#include "Snippets/ListView/ListviewRestoreScrollPosition.h.embed"
};

FrameworkElement restoring() {
#include "Snippets/ListView/ListviewRestoreScrollPosition.h"

    return gallery::controlExample({
        .header = gallery::snippet(restoringHeader),
        .example = example,
        .code = gallery::snippet(restoringCode),
    });
}

constexpr char8_t scrollingHeader[] = {
#include "Snippets/ListView/ListviewScrollIntoView.html.embed"
};
constexpr char8_t scrollingCode[] = {
#include "Snippets/ListView/ListviewScrollIntoView.h.embed"
};

FrameworkElement scrolling() {
#include "Snippets/ListView/ListviewScrollIntoView.h"

    return gallery::controlExample({
        .header = gallery::snippet(scrollingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(scrollingCode),
    });
}

constexpr char8_t draggingHeader[] = {
#include "Snippets/ListView/ListviewsDragDropReordering.html.embed"
};
constexpr char8_t draggingCode[] = {
#include "Snippets/ListView/ListviewsDragDropReordering.h.embed"
};

FrameworkElement dragging() {
#include "Snippets/ListView/ListviewsDragDropReordering.h"

    return gallery::controlExample({
        .header = gallery::snippet(draggingHeader),
        .example = example,
        .code = gallery::snippet(draggingCode),
    });
}

constexpr char8_t groupedHeader[] = {
#include "Snippets/ListView/ListviewGroupedHeaders.html.embed"
};
constexpr char8_t groupedCode[] = {
#include "Snippets/ListView/ListviewGroupedHeaders.h.embed"
};

FrameworkElement grouped() {
#include "Snippets/ListView/ListviewGroupedHeaders.h"

    return gallery::controlExample({
        .header = gallery::snippet(groupedHeader),
        .example = example,
        .code = gallery::snippet(groupedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::listViewPage() {
    return StackPanel {basicList(), selection(), images(), filtering(), menus(), messaging(), restoring(), scrolling(), dragging(), grouped()};
}
