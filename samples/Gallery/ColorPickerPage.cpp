// Страница ColorPicker — ColorPickerPage оригинала.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t propertiesHeader[] = {
#include "Snippets/ColorPicker/Properties.html.embed"
};
constexpr char8_t propertiesCode[] = {
#include "Snippets/ColorPicker/Properties.h.embed"
};

FrameworkElement properties() {
#include "Snippets/ColorPicker/Properties.h"

    return gallery::controlExample({
        .header = gallery::snippet(propertiesHeader),
        .example = picker,
        .options = {options},
        .code = gallery::snippet(propertiesCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::colorPickerPage() {
    return StackPanel {properties()};
}