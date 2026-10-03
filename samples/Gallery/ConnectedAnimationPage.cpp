// Страница ConnectedAnimation -- ConnectedAnimationPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <memory>
#include <vector>
#include "Box.h"
#include "ItemBuilder.h"
#include "ItemElement.h"
#include "generated/Microsoft.UI.Dispatching.h"
#include "MotionPages.h"
#include "PagedFrame.h"
#include "ResourceBrush.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Media.Animation.h"
#include "generated/Microsoft.UI.Xaml.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t listPageHeader[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationListPage.html.embed"
};
constexpr char8_t listPageCode[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationListPage.h.embed"
};

FrameworkElement listPage() {
#include "Snippets/ConnectedAnimation/ConnectedAnimationListPage.h"

    return gallery::controlExample({
        .header = gallery::snippet(listPageHeader),
        .example = example,
        .code = gallery::snippet(listPageCode),
    });
}

constexpr char8_t sameElementsHeader[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationElementsSame.html.embed"
};
constexpr char8_t sameElementsCode[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationElementsSame.h.embed"
};

FrameworkElement sameElements() {
#include "Snippets/ConnectedAnimation/ConnectedAnimationElementsSame.h"

    return gallery::controlExample({
        .header = gallery::snippet(sameElementsHeader),
        .example = example,
        .code = gallery::snippet(sameElementsCode),
    });
}

constexpr char8_t simpleHeader[] = {
#include "Snippets/ConnectedAnimation/SimpleConnectedAnimation.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/ConnectedAnimation/SimpleConnectedAnimation.h.embed"
};

FrameworkElement simple() {
#include "Snippets/ConnectedAnimation/SimpleConnectedAnimation.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t itemsRepeaterHeader[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationItemsrepeater.html.embed"
};
constexpr char8_t itemsRepeaterCode[] = {
#include "Snippets/ConnectedAnimation/ConnectedAnimationItemsrepeater.h.embed"
};

FrameworkElement itemsRepeater() {
#include "Snippets/ConnectedAnimation/ConnectedAnimationItemsrepeater.h"

    return gallery::controlExample({
        .header = gallery::snippet(itemsRepeaterHeader),
        .example = example,
        .code = gallery::snippet(itemsRepeaterCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::connectedAnimationPage() {
    return StackPanel {listPage(), sameElements(), simple(), itemsRepeater()};
}
