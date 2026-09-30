// Страница HyperlinkButton — HyperlinkButtonPage оригинала: два примера.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t navigateHeader[] = {
#include "Snippets/HyperlinkButton/Navigate.html.embed"
};
constexpr char8_t navigateCode[] = {
#include "Snippets/HyperlinkButton/Navigate.h.embed"
};

FrameworkElement navigateExample() {
#include "Snippets/HyperlinkButton/Navigate.h"

    return gallery::controlExample({
        .header = gallery::snippet(navigateHeader),
        .example = link,
        .options = {disable},
        .code = gallery::snippet(navigateCode),
    });
}

constexpr char8_t clickHeader[] = {
#include "Snippets/HyperlinkButton/Click.html.embed"
};
constexpr char8_t clickCode[] = {
#include "Snippets/HyperlinkButton/Click.h.embed"
};

FrameworkElement clickExample() {
    return gallery::controlExample({
        .header = gallery::snippet(clickHeader),
        .example =
#include "Snippets/HyperlinkButton/Click.h"
        ,
        .code = gallery::snippet(clickCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::hyperlinkButtonPage() {
    return StackPanel {navigateExample(), clickExample()};
}