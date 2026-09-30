// Страница VariableSizedWrapGrid — VariableSizedWrapGridPage оригинала: один пример.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlHeader[] = {
#include "Snippets/VariableSizedWrapGrid/VariableSizedWrapGridControl.html.embed"
};
constexpr char8_t controlCode[] = {
#include "Snippets/VariableSizedWrapGrid/VariableSizedWrapGridControl.h.embed"
};

FrameworkElement control() {
#include "Snippets/VariableSizedWrapGrid/VariableSizedWrapGridControl.h"

    return gallery::controlExample({
        .header = gallery::snippet(controlHeader),
        .example = grid,
        .options = {orientationGroup},
        .code = gallery::snippet(controlCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::variableSizedWrapGridPage() {
    return StackPanel {control()};
}