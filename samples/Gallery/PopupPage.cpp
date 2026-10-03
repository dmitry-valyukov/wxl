// Страница Popup -- PopupPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t offsetExampleHeader[] = {
#include "Snippets/Popup/PopupOffsetPositioning.html.embed"
};
constexpr char8_t offsetExampleCode[] = {
#include "Snippets/Popup/PopupOffsetPositioning.h.embed"
};

FrameworkElement offsetExample() {
#include "Snippets/Popup/PopupOffsetPositioning.h"

    return gallery::controlExample({
        .header = gallery::snippet(offsetExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(offsetExampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::popupPage() {
    return StackPanel {offsetExample()};
}
