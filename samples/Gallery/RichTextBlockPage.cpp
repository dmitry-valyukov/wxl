// Страница RichTextBlock — RichTextBlockPage оригинала.
//
// Четвёртый пример оригинала (RichtextblockCustomTexthighlighting:
// TextHighlighter и диапазоны TextRange) не перенесён: TextRange из
// Microsoft.UI.Xaml.Documents — структура с тем же именем, что у обёртки
// Microsoft.UI.Text.TextRange, и коллекция структур ещё не проецируется.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/RichTextBlock/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/RichTextBlock/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/RichTextBlock/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t selectionHeader[] = {
#include "Snippets/RichTextBlock/CustomSelectionHighlight.html.embed"
};
constexpr char8_t selectionCode[] = {
#include "Snippets/RichTextBlock/CustomSelectionHighlight.h.embed"
};

FrameworkElement customSelection() {
    return gallery::controlExample({
        .header = gallery::snippet(selectionHeader),
        .example =
#include "Snippets/RichTextBlock/CustomSelectionHighlight.h"
        ,
        .code = gallery::snippet(selectionCode),
    });
}

constexpr char8_t overflowHeader[] = {
#include "Snippets/RichTextBlock/Overflow.html.embed"
};
constexpr char8_t overflowCode[] = {
#include "Snippets/RichTextBlock/Overflow.h.embed"
};

FrameworkElement overflow() {
#include "Snippets/RichTextBlock/Overflow.h"

    return gallery::controlExample({
        .header = gallery::snippet(overflowHeader),
        .example = columns,
        .code = gallery::snippet(overflowCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::richTextBlockPage() {
    return StackPanel {simple(), customSelection(), overflow()};
}