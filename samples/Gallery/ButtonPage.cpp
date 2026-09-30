// Страница Button — ButtonPage оригинала (Samples/Button): четыре примера,
// те же, что там. Код каждого — файл в Snippets/Button/ (см. Pages.h).

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/Button/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/Button/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/Button/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = button,
        .output = {output},
        .options = {disable},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t withImageHeader[] = {
#include "Snippets/Button/WithImage.html.embed"
};
constexpr char8_t withImageCode[] = {
#include "Snippets/Button/WithImage.h.embed"
};

FrameworkElement withImage() {
#include "Snippets/Button/WithImage.h"

    return gallery::controlExample({
        .header = gallery::snippet(withImageHeader),
        .example = StackPanel {orientation.horizontal, button},
        .output = {output},
        .code = gallery::snippet(withImageCode),
    });
}

constexpr char8_t stylesHeader[] = {
#include "Snippets/Button/BuiltInStyles.html.embed"
};
constexpr char8_t stylesCode[] = {
#include "Snippets/Button/BuiltInStyles.h.embed"
};

FrameworkElement builtInStyles() {
    return gallery::controlExample({
        .header = gallery::snippet(stylesHeader),
        .example =
#include "Snippets/Button/BuiltInStyles.h"
        ,
        .code = gallery::snippet(stylesCode),
    });
}

constexpr char8_t wrappingHeader[] = {
#include "Snippets/Button/Wrapping.html.embed"
};
constexpr char8_t wrappingCode[] = {
#include "Snippets/Button/Wrapping.h.embed"
};

FrameworkElement wrapping() {
    return gallery::controlExample({
        .header = gallery::snippet(wrappingHeader),
        .example =
#include "Snippets/Button/Wrapping.h"
        ,
        .code = gallery::snippet(wrappingCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::buttonPage() {
    return StackPanel {simple(), withImage(), builtInStyles(), wrapping()};
}
