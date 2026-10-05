// Страница ThemeShadow -- ThemeShadowPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t appliedBorderHeader[] = {
#include "Snippets/ThemeShadow/AppliedBorder.html.embed"
};
constexpr char8_t appliedBorderCode[] = {
#include "Snippets/ThemeShadow/AppliedBorder.h.embed"
};

FrameworkElement appliedBorder() {
#include "Snippets/ThemeShadow/AppliedBorder.h"

    return gallery::controlExample({
        .header = gallery::snippet(appliedBorderHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(appliedBorderCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::themeShadowPage() {
    return StackPanel {appliedBorder()};
}
