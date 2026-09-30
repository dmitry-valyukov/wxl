// Страница ToggleSwitch — ToggleSwitchPage оригинала: два примера.

#include "Pages.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/ToggleSwitch/Simple.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/ToggleSwitch/Simple.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/ToggleSwitch/Simple.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t customHeader[] = {
#include "Snippets/ToggleSwitch/Custom.html.embed"
};
constexpr char8_t customCode[] = {
#include "Snippets/ToggleSwitch/Custom.h.embed"
};

FrameworkElement custom() {
    return gallery::controlExample({
        .header = gallery::snippet(customHeader),
        .example =
#include "Snippets/ToggleSwitch/Custom.h"
        ,
        .code = gallery::snippet(customCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::toggleSwitchPage() {
    return StackPanel {simple(), custom()};
}