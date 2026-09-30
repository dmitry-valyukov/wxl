// Страница TextBlock — TextBlockPage оригинала: пять примеров.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/TextBlock/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/TextBlock/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/TextBlock/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t styleHeader[] = {
#include "Snippets/TextBlock/StyleApplied.html.embed"
};
constexpr char8_t styleCode[] = {
#include "Snippets/TextBlock/StyleApplied.h.embed"
};

FrameworkElement styleApplied() {
#include "Snippets/TextBlock/StyleApplied.h"

    return gallery::controlExample({
        .header = gallery::snippet(styleHeader),
        .example = styled,
        .code = gallery::snippet(styleCode),
    });
}

constexpr char8_t variousHeader[] = {
#include "Snippets/TextBlock/VariousProperties.html.embed"
};
constexpr char8_t variousCode[] = {
#include "Snippets/TextBlock/VariousProperties.h.embed"
};

FrameworkElement various() {
    return gallery::controlExample({
        .header = gallery::snippet(variousHeader),
        .example =
#include "Snippets/TextBlock/VariousProperties.h"
        ,
        .code = gallery::snippet(variousCode),
    });
}

constexpr char8_t inlineHeader[] = {
#include "Snippets/TextBlock/InlineTextElements.html.embed"
};
constexpr char8_t inlineCode[] = {
#include "Snippets/TextBlock/InlineTextElements.h.embed"
};

FrameworkElement inlineElements() {
    return gallery::controlExample({
        .header = gallery::snippet(inlineHeader),
        .example =
#include "Snippets/TextBlock/InlineTextElements.h"
        ,
        .code = gallery::snippet(inlineCode),
    });
}

constexpr char8_t selectableHeader[] = {
#include "Snippets/TextBlock/Selectable.html.embed"
};
constexpr char8_t selectableCode[] = {
#include "Snippets/TextBlock/Selectable.h.embed"
};

FrameworkElement selectable() {
#include "Snippets/TextBlock/Selectable.h"

    return gallery::controlExample({
        .header = gallery::snippet(selectableHeader),
        .example = text,
        .options = {option},
        .code = gallery::snippet(selectableCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::textBlockPage() {
    return StackPanel {simple(), styleApplied(), various(), inlineElements(), selectable()};
}