// Страница TextBox — TextBoxPage оригинала: четыре примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/TextBox/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/TextBox/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/TextBox/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t headerHeader[] = {
#include "Snippets/TextBox/HeaderPlaceholderText.html.embed"
};
constexpr char8_t headerCode[] = {
#include "Snippets/TextBox/HeaderPlaceholderText.h.embed"
};

FrameworkElement headerPlaceholder() {
    return gallery::controlExample({
        .header = gallery::snippet(headerHeader),
        .example =
#include "Snippets/TextBox/HeaderPlaceholderText.h"
        ,
        .code = gallery::snippet(headerCode),
    });
}

constexpr char8_t readOnlyHeader[] = {
#include "Snippets/TextBox/ReadOnlyVarious.html.embed"
};
constexpr char8_t readOnlyCode[] = {
#include "Snippets/TextBox/ReadOnlyVarious.h.embed"
};

FrameworkElement readOnlyVarious() {
    return gallery::controlExample({
        .header = gallery::snippet(readOnlyHeader),
        .example =
#include "Snippets/TextBox/ReadOnlyVarious.h"
        ,
        .code = gallery::snippet(readOnlyCode),
    });
}

constexpr char8_t multiHeader[] = {
#include "Snippets/TextBox/MultiLineSpell.html.embed"
};
constexpr char8_t multiCode[] = {
#include "Snippets/TextBox/MultiLineSpell.h.embed"
};

FrameworkElement multiLine() {
    return gallery::controlExample({
        .header = gallery::snippet(multiHeader),
        .example =
#include "Snippets/TextBox/MultiLineSpell.h"
        ,
        .code = gallery::snippet(multiCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::textBoxPage() {
    return StackPanel {simple(), headerPlaceholder(), readOnlyVarious(), multiLine()};
}