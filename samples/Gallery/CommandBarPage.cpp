// Страница CommandBar -- CommandBarPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t labelsSideHeader[] = {
#include "Snippets/CommandBar/LabelsSide.html.embed"
};
constexpr char8_t labelsSideCode[] = {
#include "Snippets/CommandBar/LabelsSide.h.embed"
};

FrameworkElement labelsSide() {
#include "Snippets/CommandBar/LabelsSide.h"

    return gallery::controlExample({
        .header = gallery::snippet(labelsSideHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(labelsSideCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::commandBarPage() {
    return StackPanel {labelsSide()};
}
