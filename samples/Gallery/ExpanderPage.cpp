// Страница Expander — ExpanderPage оригинала: два примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t textHeader[] = {
#include "Snippets/Expander/ExpanderTextHeaderContent.html.embed"
};
constexpr char8_t textCode[] = {
#include "Snippets/Expander/ExpanderTextHeaderContent.h.embed"
};

FrameworkElement textHeaderContent() {
#include "Snippets/Expander/ExpanderTextHeaderContent.h"

    return gallery::controlExample({
        .header = gallery::snippet(textHeader),
        .example = expander,
        .options = {direction},
        .code = gallery::snippet(textCode),
    });
}

constexpr char8_t alignmentHeader[] = {
#include "Snippets/Expander/ModifyingExpandersContentAlignment.html.embed"
};
constexpr char8_t alignmentCode[] = {
#include "Snippets/Expander/ModifyingExpandersContentAlignment.h.embed"
};

FrameworkElement contentAlignment() {
    return gallery::controlExample({
        .header = gallery::snippet(alignmentHeader),
        .example =
#include "Snippets/Expander/ModifyingExpandersContentAlignment.h"
        ,
        .code = gallery::snippet(alignmentCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::expanderPage() {
    return StackPanel {textHeaderContent(), contentAlignment()};
}