// Страница SettingsCard Community Toolkit: контрол wxl (см. decisions/0465), примеры и их исходники.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/SettingsCard/SettingsCardBasic.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/SettingsCard/SettingsCardBasic.h.embed"
};

FrameworkElement basicExample() {
#include "Snippets/SettingsCard/SettingsCardBasic.h"

    return gallery::controlExample({.header = gallery::snippet(basicHeader), .example = example, .code = gallery::snippet(basicCode)});
}

constexpr char8_t clickableHeader[] = {
#include "Snippets/SettingsCard/SettingsCardClickable.html.embed"
};
constexpr char8_t clickableCode[] = {
#include "Snippets/SettingsCard/SettingsCardClickable.h.embed"
};

FrameworkElement clickableExample() {
#include "Snippets/SettingsCard/SettingsCardClickable.h"

    return gallery::controlExample({.header = gallery::snippet(clickableHeader), .example = example, .code = gallery::snippet(clickableCode)});
}

constexpr char8_t alignmentHeader[] = {
#include "Snippets/SettingsCard/SettingsCardAlignment.html.embed"
};
constexpr char8_t alignmentCode[] = {
#include "Snippets/SettingsCard/SettingsCardAlignment.h.embed"
};

FrameworkElement alignmentExample() {
#include "Snippets/SettingsCard/SettingsCardAlignment.h"

    return gallery::controlExample({.header = gallery::snippet(alignmentHeader), .example = example, .code = gallery::snippet(alignmentCode)});
}

}  // namespace

wxl::FrameworkElement gallery::settingsCardPage() {
    return StackPanel {basicExample(), clickableExample(), alignmentExample()};
}
