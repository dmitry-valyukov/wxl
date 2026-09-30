// Страница ToggleSplitButton — ToggleSplitButtonPage оригинала.

#include "Pages.h"
#include "event_awaitable.h"

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t listHeader[] = {
#include "Snippets/ToggleSplitButton/BulletList.html.embed"
};
constexpr char8_t listCode[] = {
#include "Snippets/ToggleSplitButton/BulletList.h.embed"
};

FrameworkElement listExample() {
#include "Snippets/ToggleSplitButton/BulletList.h"

    return gallery::controlExample({
        .header = gallery::snippet(listHeader),
        .example = listButton,
        .options = {richBox},
        .code = gallery::snippet(listCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::toggleSplitButtonPage() {
    return StackPanel {listExample()};
}