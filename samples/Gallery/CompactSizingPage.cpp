// Страница CompactSizing -- CompactSizingPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlsHeader[] = {
#include "Snippets/CompactSizing/CompactSizingControls.html.embed"
};
constexpr char8_t controlsCode[] = {
#include "Snippets/CompactSizing/CompactSizingControls.h.embed"
};

FrameworkElement controls() {
#include "Snippets/CompactSizing/CompactSizingControls.h"

    return gallery::controlExample({
        .header = gallery::snippet(controlsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(controlsCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::compactSizingPage() {
    return StackPanel {
        RichTextBlock {
            Margin {0, 24, 0, 0},
            Paragraph {
                Run {u"Controls that support compact styling:", FontWeight {600}},
                LineBreak {},
                Run {u"\u2022 ListView"},
                LineBreak {},
                Run {u"\u2022 TextBox"},
                LineBreak {},
                Run {u"\u2022 PasswordBox"},
                LineBreak {},
                Run {u"\u2022 AutoSuggestBox"},
                LineBreak {},
                Run {u"\u2022 ComboBox"},
                LineBreak {},
                Run {u"\u2022 DatePicker"},
                LineBreak {},
                Run {u"\u2022 TimePicker"},
                LineBreak {},
                Run {u"\u2022 TreeView"},
                LineBreak {},
                Run {u"\u2022 NavigationView"},
                LineBreak {},
                Run {u"\u2022 MenuBar"},
            },
        },controls()};
}
