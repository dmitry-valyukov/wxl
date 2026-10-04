// Страница PullToRefresh -- PullToRefreshPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "StringList.h"
#include <wxl/Windows.Foundation.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/PullToRefresh/BasicPulltorefresh.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/PullToRefresh/BasicPulltorefresh.h.embed"
};

FrameworkElement basic() {
#include "Snippets/PullToRefresh/BasicPulltorefresh.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t customIconHeader[] = {
#include "Snippets/PullToRefresh/CustomIconPulltorefresh.html.embed"
};
constexpr char8_t customIconCode[] = {
#include "Snippets/PullToRefresh/CustomIconPulltorefresh.h.embed"
};

FrameworkElement customIcon() {
#include "Snippets/PullToRefresh/CustomIconPulltorefresh.h"

    return gallery::controlExample({
        .header = gallery::snippet(customIconHeader),
        .example = example,
        .code = gallery::snippet(customIconCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::pullToRefreshPage() {
    return StackPanel {basic(), customIcon()};
}
