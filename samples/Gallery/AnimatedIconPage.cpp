// Страница AnimatedIcon -- AnimatedIconPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "generated/Microsoft.UI.Xaml.Controls.AnimatedVisuals.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t buttonHeader[] = {
#include "Snippets/AnimatedIcon/AddingAnimatediconButton.html.embed"
};
constexpr char8_t buttonCode[] = {
#include "Snippets/AnimatedIcon/AddingAnimatediconButton.h.embed"
};

FrameworkElement button() {
#include "Snippets/AnimatedIcon/AddingAnimatediconButton.h"

    return gallery::controlExample({
        .header = gallery::snippet(buttonHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(buttonCode),
    });
}

constexpr char8_t navigationViewHeader[] = {
#include "Snippets/AnimatedIcon/AddingAnimatediconNavigationview.html.embed"
};
constexpr char8_t navigationViewCode[] = {
#include "Snippets/AnimatedIcon/AddingAnimatediconNavigationview.h.embed"
};

FrameworkElement navigationView() {
#include "Snippets/AnimatedIcon/AddingAnimatediconNavigationview.h"

    return gallery::controlExample({
        .header = gallery::snippet(navigationViewHeader),
        .example = example,
        .code = gallery::snippet(navigationViewCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::animatedIconPage() {
    return StackPanel {button(), navigationView()};
}
