// Страница LayoutPanel -- LayoutPanelPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "CustomLayout.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t customLayoutHeader[] = {
#include "Snippets/LayoutPanel/CustomLayout.html.embed"
};
constexpr char8_t customLayoutCode[] = {
#include "Snippets/LayoutPanel/CustomLayout.h.embed"
};

FrameworkElement customLayout() {
#include "Snippets/LayoutPanel/CustomLayout.h"

    return gallery::controlExample({
        .header = gallery::snippet(customLayoutHeader),
        .example = panel,
        .options = {options},
        .code = gallery::snippet(customLayoutCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::layoutPanelPage() {
    return StackPanel {customLayout()};
}
