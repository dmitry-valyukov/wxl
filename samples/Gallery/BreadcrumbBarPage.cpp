// Страница BreadcrumbBar -- BreadcrumbBarPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <string>
#include <vector>
#include "Announce.h"
#include "Box.h"
#include "BoundTemplate.h"
#include "StringList.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControl.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControl.h.embed"
};

FrameworkElement basic() {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControl.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t customTemplateHeader[] = {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControlCustomDatatemplate.html.embed"
};
constexpr char8_t customTemplateCode[] = {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControlCustomDatatemplate.h.embed"
};

FrameworkElement customTemplate() {
#include "Snippets/BreadcrumbBar/BreadcrumbbarControlCustomDatatemplate.h"

    return gallery::controlExample({
        .header = gallery::snippet(customTemplateHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(customTemplateCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::breadcrumbBarPage() {
    return StackPanel {basic(), customTemplate()};
}
