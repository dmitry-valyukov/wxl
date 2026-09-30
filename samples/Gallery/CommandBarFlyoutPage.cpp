// Страница CommandBarFlyout -- CommandBarFlyoutPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t onAppObjectHeader[] = {
#include "Snippets/CommandBarFlyout/OnAppObject.html.embed"
};
constexpr char8_t onAppObjectCode[] = {
#include "Snippets/CommandBarFlyout/OnAppObject.h.embed"
};

FrameworkElement onAppObject() {
#include "Snippets/CommandBarFlyout/OnAppObject.h"

    return gallery::controlExample({
        .header = gallery::snippet(onAppObjectHeader),
        .example = example,
        .code = gallery::snippet(onAppObjectCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::commandBarFlyoutPage() {
    return StackPanel {onAppObject()};
}
