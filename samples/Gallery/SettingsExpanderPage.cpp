// Страница SettingsExpander Community Toolkit: контрол wxl (см. decisions/0465), примеры и их исходники.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/SettingsExpander/SettingsExpanderBasic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/SettingsExpander/SettingsExpanderBasic.h.embed"
};

FrameworkElement basicExample() {
#include "Snippets/SettingsExpander/SettingsExpanderBasic.h"

    return gallery::controlExample({.header = gallery::snippet(basicHeader), .example = example, .code = gallery::snippet(basicCode)});
}

}  // namespace

wxl::FrameworkElement gallery::settingsExpanderPage() {
    return StackPanel {basicExample()};
}
