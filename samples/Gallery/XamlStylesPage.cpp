// Страница XamlStyles — XamlStylesPage оригинала. Стиль XAML — набор значений свойств для одного типа; у wxl это
// пресет (Preset): значения, лежащие в переменной. Неявного стиля описание не знает — его роль у функции,
// делающей контрол.

#include "Pages.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t applyingHeader[] = {
#include "Snippets/XamlStyles/XamlStylesCreatingApplyingStyle.html.embed"
};
constexpr char8_t applyingCode[] = {
#include "Snippets/XamlStyles/XamlStylesCreatingApplyingStyle.h.embed"
};
constexpr char8_t implicitHeader[] = {
#include "Snippets/XamlStyles/XamlStylesStyleWithoutKeyImplicit.html.embed"
};
constexpr char8_t implicitCode[] = {
#include "Snippets/XamlStyles/XamlStylesStyleWithoutKeyImplicit.h.embed"
};

FrameworkElement applying() {
#include "Snippets/XamlStyles/XamlStylesCreatingApplyingStyle.h"
    return gallery::controlExample({.header = gallery::snippet(applyingHeader), .example = example, .code = gallery::snippet(applyingCode)});
}

FrameworkElement implicit() {
#include "Snippets/XamlStyles/XamlStylesStyleWithoutKeyImplicit.h"
    return gallery::controlExample({.header = gallery::snippet(implicitHeader), .example = example, .code = gallery::snippet(implicitCode)});
}

}  // namespace

FrameworkElement gallery::xamlStylesPage() {
    return StackPanel {
        spacing = 12.0,
        TextBlock {Margin {0, 12, 0, 4}, styles.TextBlock.Subtitle, u"Creating and using styles"},
        RichTextBlock {
            Paragraph {Run {u"The definition of styles is similar to other resources: app-level, page-level, control-level."}},
        },
        RichTextBlock {
            Paragraph {Run {u"• "}, Bold {Run {u"Styles"}}, Run {u" are reusable collections of property settings for a specific control type."}},
            Paragraph {Run {u"• A "}, Bold {Run {u"keyed style"}}, Run {u" is used for explicit application, while an "}, Bold {Run {u"implicit style"}},
                       Run {u" is used for automatic application to all controls of a type."}},
            Paragraph {Run {u"• Styles improve maintainability, consistency, and reduce repetition in the code."}},
        },
        applying(),
        implicit(),
    };
}
