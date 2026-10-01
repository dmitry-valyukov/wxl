// Страница RichEditBox — RichEditBoxPage оригинала.
//
// Не перенесено: пример с CommandBarFlyout («Share» в меню выделения) —
// нужны SelectionFlyout, ContextFlyout и PrimaryCommands, а коллекция
// ICommandBarElement ещё не проецируется; кнопки «Open» и «Save» примера
// с редактором — им нужны выбор файла и потоки WinRT.

#include "Pages.h"
#include "event_awaitable.h"
#include "generated/Microsoft.UI.Xaml.Shapes.h"

#include <limits>

import wxl.async;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t simpleHeader[] = {
#include "Snippets/RichEditBox/SimpleTextEditor.html.embed"
};
constexpr char8_t simpleCode[] = {
#include "Snippets/RichEditBox/SimpleTextEditor.h.embed"
};

FrameworkElement simple() {
    return gallery::controlExample({
        .header = gallery::snippet(simpleHeader),
        .example =
#include "Snippets/RichEditBox/SimpleTextEditor.h"
        ,
        .code = gallery::snippet(simpleCode),
    });
}

constexpr char8_t customHeader[] = {
#include "Snippets/RichEditBox/CustomEditor.html.embed"
};
constexpr char8_t customCode[] = {
#include "Snippets/RichEditBox/CustomEditor.h.embed"
};

FrameworkElement customEditor() {
#include "Snippets/RichEditBox/CustomEditor.h"

    return gallery::controlExample({
        .header = gallery::snippet(customHeader),
        .example = Grid {rowDefinitions = u"auto,*,auto", toolbar, Border {row = 1, editor}, Border {row = 2, findBar}},
        .code = gallery::snippet(customCode),
    });
}

constexpr char8_t mathHeader[] = {
#include "Snippets/RichEditBox/Math.html.embed"
};
constexpr char8_t mathCode[] = {
#include "Snippets/RichEditBox/Math.h.embed"
};

FrameworkElement math() {
#include "Snippets/RichEditBox/Math.h"

    return gallery::controlExample({
        .header = gallery::snippet(mathHeader),
        .example = StackPanel {spacing = 8.0, description, editor},
        .code = gallery::snippet(mathCode),
    });
}

constexpr char8_t mathmlHeader[] = {
#include "Snippets/RichEditBox/WorkingMathml.html.embed"
};
constexpr char8_t mathmlCode[] = {
#include "Snippets/RichEditBox/WorkingMathml.h.embed"
};

FrameworkElement workingMathml() {
#include "Snippets/RichEditBox/WorkingMathml.h"

    return gallery::controlExample({
        .header = gallery::snippet(mathmlHeader),
        .example = StackPanel {
            spacing = 16.0,
            editor,
            TextBlock {u"MathML Code", FontWeight {600}, Margin {0, 0, 0, -8}},
            Border {
                CornerRadius {4},
                background = brushes.Card.BackgroundFillColor.Default,
                Padding {8, 8, 8, 0},
                ScrollViewer {maxHeight = 450, content = mathml},
            },
        },
        .options = {setFormula},
        .code = gallery::snippet(mathmlCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::richEditBoxPage() {
    return StackPanel {simple(), customEditor(), math(), workingMathml()};
}