// Страница AutoSuggestBox — AutoSuggestBoxPage оригинала: два примера.
//
// Подсказки оригинала — строки и объекты ControlInfoDataItem; здесь везде
// строки (названия), а по выбранному названию контрол находится в каталоге.

#include "Pages.h"
#include "StringList.h"

#include <string>
#include <vector>

using namespace wxl;
using namespace wxl::dsl;

namespace {

#include "Snippets/AutoSuggestBox/Cats.h"

constexpr char8_t basicHeader[] = {
#include "Snippets/AutoSuggestBox/Basic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/AutoSuggestBox/Basic.h.embed"
};

FrameworkElement basic() {
#include "Snippets/AutoSuggestBox/Basic.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = StackPanel {orientation.horizontal, box, output},
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t searchHeader[] = {
#include "Snippets/AutoSuggestBox/SearchBox.html.embed"
};
constexpr char8_t searchCode[] = {
#include "Snippets/AutoSuggestBox/SearchBox.h.embed"
};

FrameworkElement searchBox() {
#include "Snippets/AutoSuggestBox/SearchBox.h"

    return gallery::controlExample({
        .header = gallery::snippet(searchHeader),
        .example = Grid {rowDefinitions = u"auto,auto", box, Border {row = 1, details}},
        .code = gallery::snippet(searchCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::autoSuggestBoxPage() {
    return StackPanel {basic(), searchBox()};
}