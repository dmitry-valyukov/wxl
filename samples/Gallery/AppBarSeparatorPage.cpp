// Страница AppBarSeparator -- AppBarSeparatorPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t separatedHeader[] = {
#include "Snippets/AppBarSeparator/Separated.html.embed"
};
constexpr char8_t separatedCode[] = {
#include "Snippets/AppBarSeparator/Separated.h.embed"
};

FrameworkElement separated() {
    return gallery::controlExample({
        .header = gallery::snippet(separatedHeader),
        .example =
#include "Snippets/AppBarSeparator/Separated.h"
        ,
        .code = gallery::snippet(separatedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::appBarSeparatorPage() {
    return StackPanel {separated()};
}
