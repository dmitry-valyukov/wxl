// Страница RepeatButton — RepeatButtonPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/RepeatButton/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/RepeatButton/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/RepeatButton/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {orientation.horizontal, button, output},
        .options = {disable},
        .code = gallery::snippet(simpleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::repeatButtonPage() {
    return StackPanel {simple()};
}