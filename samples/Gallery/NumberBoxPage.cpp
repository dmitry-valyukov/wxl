// Страница NumberBox — NumberBoxPage оригинала: три примера.

#include "Pages.h"
#include "Shell.h"

#include <limits>

#include <wxl/Windows.Globalization.NumberFormatting.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t expressionsHeader[] = {
#include "Snippets/NumberBox/EvaluatesExpressions.html.embed"
};
constexpr char8_t expressionsCode[] = {
#include "Snippets/NumberBox/EvaluatesExpressions.h.embed"
};

FrameworkElement expressions() {
    return gallery::controlExample({
        .header = gallery::snippet(expressionsHeader),
        .example =
#include "Snippets/NumberBox/EvaluatesExpressions.h"
        ,
        .code = gallery::snippet(expressionsCode),
    });
}

constexpr char8_t spinHeader[] = {
#include "Snippets/NumberBox/SpinButton.html.embed"
};
constexpr char8_t spinCode[] = {
#include "Snippets/NumberBox/SpinButton.h.embed"
};

FrameworkElement spinButton() {
#include "Snippets/NumberBox/SpinButton.h"

    return gallery::controlExample({
        .header = gallery::snippet(spinHeader),
        .example = number,
        .options = {placement},
        .code = gallery::snippet(spinCode),
    });
}

constexpr char8_t formattedHeader[] = {
#include "Snippets/NumberBox/FormattedRoundsNearest.html.embed"
};
constexpr char8_t formattedCode[] = {
#include "Snippets/NumberBox/FormattedRoundsNearest.h.embed"
};

FrameworkElement formatted() {
#include "Snippets/NumberBox/FormattedRoundsNearest.h"

    return gallery::controlExample({
        .header = gallery::snippet(formattedHeader),
        .example = amount,
        .code = gallery::snippet(formattedCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::numberBoxPage() {
    return StackPanel {expressions(), spinButton(), formatted()};
}