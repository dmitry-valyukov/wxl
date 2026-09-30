// Страница RatingControl — RatingControlPage оригинала: два примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/RatingControl/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/RatingControl/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/RatingControl/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {vAlign.top, rating},
        .output = {output},
        .options = {options},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t placeholderHeader[] = {
#include "Snippets/RatingControl/Placeholder.html.embed"
};
constexpr char8_t placeholderCode[] = {
#include "Snippets/RatingControl/Placeholder.h.embed"
};

FrameworkElement placeholder() {
#include "Snippets/RatingControl/Placeholder.h"

    return gallery::controlExample({
        .header = gallery::snippet(placeholderHeader),
        .example = rating,
        .options = {StackPanel {width = 220, slider}},
        .code = gallery::snippet(placeholderCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::ratingControlPage() {
    return StackPanel {simple(), placeholder()};
}