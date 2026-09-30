// Страница Slider — SliderPage оригинала: четыре примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/Slider/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/Slider/Simple.h.embed"
};

FrameworkElement simple() {
#include "Snippets/Slider/Simple.h"

    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example = StackPanel {orientation.horizontal, slider},
        .output = {output},
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t rangeHeader[] = {
#include "Snippets/Slider/Range.html.embed"
};
constexpr char8_t rangeCode[] = {
#include "Snippets/Slider/Range.h.embed"
};

FrameworkElement range() {
#include "Snippets/Slider/Range.h"

    return gallery::controlExample({
        .header = gallery::snippet(rangeHeader),
        .example = StackPanel {vAlign.center, orientation.horizontal, slider},
        .output = {output},
        .options = {options},
        .code = gallery::snippet(rangeCode),
    });
}

constexpr char8_t ticksHeader[] = {
#include "Snippets/Slider/Ticks.html.embed"
};
constexpr char8_t ticksCode[] = {
#include "Snippets/Slider/Ticks.h.embed"
};

FrameworkElement ticks() {
#include "Snippets/Slider/Ticks.h"

    return gallery::controlExample({
        .header = gallery::snippet(ticksHeader),
        .example = StackPanel {orientation.horizontal, slider},
        .output = {output},
        .options = {StackPanel {snaps}},
        .code = gallery::snippet(ticksCode),
    });
}

constexpr char8_t verticalHeader[] = {
#include "Snippets/Slider/Vertical.html.embed"
};
constexpr char8_t verticalCode[] = {
#include "Snippets/Slider/Vertical.h.embed"
};

FrameworkElement vertical() {
#include "Snippets/Slider/Vertical.h"

    return gallery::controlExample({
        .header = gallery::snippet(verticalHeader),
        .example = StackPanel {orientation.horizontal, slider},
        .output = {output},
        .code = gallery::snippet(verticalCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::sliderPage() {
    return StackPanel {simple(), range(), ticks(), vertical()};
}