// Страница PasswordBox — PasswordBoxPage оригинала: три примера.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/PasswordBox/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/PasswordBox/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/PasswordBox/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {box, output},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t headerHeader[] = {
#include "Snippets/PasswordBox/HeaderPlaceholderText.html.embed"
};
constexpr char8_t headerCode[] = {
#include "Snippets/PasswordBox/HeaderPlaceholderText.h.embed"
};

FrameworkElement headerPlaceholder() {
    return gallery::controlExample({
        .header = gallery::snippet(headerHeader),
        .example =
#include "Snippets/PasswordBox/HeaderPlaceholderText.h"
        ,
        .code = gallery::snippet(headerCode),
    });
}

constexpr char8_t revealHeader[] = {
#include "Snippets/PasswordBox/RevealMode.html.embed"
};
constexpr char8_t revealCode[] = {
#include "Snippets/PasswordBox/RevealMode.h.embed"
};

FrameworkElement revealMode() {
#include "Snippets/PasswordBox/RevealMode.h"

    return gallery::controlExample({
        .header = gallery::snippet(revealHeader),
        .example = StackPanel {orientation.horizontal, box, reveal},
        .code = gallery::snippet(revealCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::passwordBoxPage() {
    return StackPanel {simple(), headerPlaceholder(), revealMode()};
}