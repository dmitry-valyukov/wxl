// Страница ItemsRepeater -- ItemsRepeaterPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <memory>
#include <vector>
#include "Box.h"
#include "ItemBuilder.h"
#include "Layouts.h"
#include "RepeaterData.h"
#include "generated/Microsoft.UI.Dispatching.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/Microsoft.UI.Xaml.Input.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Microsoft.UI.Composition.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t barsHeader[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterBasicNonInteractiveItems.html.embed"
};
constexpr char8_t barsCode[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterBasicNonInteractiveItems.h.embed"
};

FrameworkElement bars() {
#include "Snippets/ItemsRepeater/ItemsRepeaterBasicNonInteractiveItems.h"

    return gallery::controlExample({
        .header = gallery::snippet(barsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(barsCode),
    });
}

constexpr char8_t virtualizingHeader[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizingScrollableListItems.html.embed"
};
constexpr char8_t virtualizingCode[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizingScrollableListItems.h.embed"
};

FrameworkElement virtualizing() {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizingScrollableListItems.h"

    return gallery::controlExample({
        .header = gallery::snippet(virtualizingHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(virtualizingCode),
    });
}

constexpr char8_t flowHeader[] = {
#include "Snippets/ItemsRepeater/FlowLayoutVirtualization.html.embed"
};
constexpr char8_t flowCode[] = {
#include "Snippets/ItemsRepeater/FlowLayoutVirtualization.h.embed"
};

FrameworkElement flow() {
#include "Snippets/ItemsRepeater/FlowLayoutVirtualization.h"

    return gallery::controlExample({
        .header = gallery::snippet(flowHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(flowCode),
    });
}

constexpr char8_t mixedHeader[] = {
#include "Snippets/ItemsRepeater/ItemsrepeaterMixedTypeCollection.html.embed"
};
constexpr char8_t mixedCode[] = {
#include "Snippets/ItemsRepeater/ItemsrepeaterMixedTypeCollection.h.embed"
};

FrameworkElement mixed() {
#include "Snippets/ItemsRepeater/ItemsrepeaterMixedTypeCollection.h"

    return gallery::controlExample({
        .header = gallery::snippet(mixedHeader),
        .example = example,
        .code = gallery::snippet(mixedCode),
    });
}

constexpr char8_t nestedHeader[] = {
#include "Snippets/ItemsRepeater/LayingOutNestedItemsrepeaters.html.embed"
};
constexpr char8_t nestedCode[] = {
#include "Snippets/ItemsRepeater/LayingOutNestedItemsrepeaters.h.embed"
};

FrameworkElement nested() {
#include "Snippets/ItemsRepeater/LayingOutNestedItemsrepeaters.h"

    return gallery::controlExample({
        .header = gallery::snippet(nestedHeader),
        .example = example,
        .code = gallery::snippet(nestedCode),
    });
}

constexpr char8_t animatedHeader[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterAnimatedScrollingContentDisplay.html.embed"
};
constexpr char8_t animatedCode[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterAnimatedScrollingContentDisplay.h.embed"
};

FrameworkElement animated() {
#include "Snippets/ItemsRepeater/ItemsRepeaterAnimatedScrollingContentDisplay.h"

    return gallery::controlExample({
        .header = gallery::snippet(animatedHeader),
        .example = example,
        .code = gallery::snippet(animatedCode),
    });
}

constexpr char8_t contentHeavyHeader[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizedContentHeavyLayout.html.embed"
};
constexpr char8_t contentHeavyCode[] = {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizedContentHeavyLayout.h.embed"
};

FrameworkElement contentHeavy() {
#include "Snippets/ItemsRepeater/ItemsRepeaterVirtualizedContentHeavyLayout.h"

    return gallery::controlExample({
        .header = gallery::snippet(contentHeavyHeader),
        .example = example,
        .code = gallery::snippet(contentHeavyCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::itemsRepeaterPage() {
    return StackPanel {bars(), virtualizing(), flow(), mixed(), nested(), animated(), contentHeavy()};
}
