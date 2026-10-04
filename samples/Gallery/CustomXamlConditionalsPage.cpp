// Страница CustomXamlConditionals — CustomXamlConditionalsPage оригинала. Условие XAML (IXamlCondition) считается
// один раз при разборе разметки; у wxl разметки нет, условие — обычное условие программы, и оно считается заново, когда
// флаг меняется. Два флага оригинала — два переключателя вверху.

#include "Pages.h"
#include "Shell.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

struct Flags {
    core::observable<bool> newExperience {true};
    core::observable<bool> legacyMode {false};
};

constexpr char8_t elementsHeader[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalElements.html.embed"
};
constexpr char8_t elementsCode[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalElements.h.embed"
};
constexpr char8_t attributesHeader[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalAttributes.html.embed"
};
constexpr char8_t attributesCode[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalAttributes.h.embed"
};
constexpr char8_t settersHeader[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalSettersStyle.html.embed"
};
constexpr char8_t settersCode[] = {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalSettersStyle.h.embed"
};

FrameworkElement elements(std::shared_ptr<Flags> const& flags) {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalElements.h"
    return gallery::controlExample({.header = gallery::snippet(elementsHeader), .example = example, .code = gallery::snippet(elementsCode)});
}

FrameworkElement attributes(std::shared_ptr<Flags> const& flags) {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalAttributes.h"
    return gallery::controlExample({.header = gallery::snippet(attributesHeader), .example = example, .code = gallery::snippet(attributesCode)});
}

FrameworkElement setters(std::shared_ptr<Flags> const& flags) {
#include "Snippets/CustomXamlConditionals/CustomXamlConditionalsConditionalSettersStyle.h"
    return gallery::controlExample({.header = gallery::snippet(settersHeader), .example = example, .code = gallery::snippet(settersCode)});
}

}  // namespace

FrameworkElement gallery::customXamlConditionalsPage() {
    auto const flags = gallery::hold<Flags>();
    return StackPanel {
        spacing = 12.0,
        InfoBar {
            title = u"Evaluated by the program",
            Margin {0, 24, 0, 0},
            isClosable = false,
            isOpen = true,
            severity = InfoBarSeverity::Informational,
            message = u"A XAML condition (IXamlCondition) is evaluated once, when the markup is parsed. Here the conditions are conditions of "
                      u"the program, so the flags below change the examples at once.",
        },
        StackPanel {orientation.horizontal, spacing = 24.0,
                    ToggleSwitch {header = u"NewExperience", isOn = Bind {flags->newExperience}},
                    ToggleSwitch {header = u"LegacyMode", isOn = Bind {flags->legacyMode}}},
        elements(flags),
        attributes(flags),
        setters(flags),
    };
}
