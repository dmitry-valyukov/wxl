// Страница ContentIsland -- ContentIslandPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "generated/Microsoft.UI.Content.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.Hosting.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/ContentIsland/BasicContentIslandContent.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/ContentIsland/BasicContentIslandContent.h.embed"
};

FrameworkElement basic() {
#include "Snippets/ContentIsland/BasicContentIslandContent.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::contentIslandPage() {
    return StackPanel {basic()};
}
