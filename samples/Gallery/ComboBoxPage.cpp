// Страница ComboBox — ComboBoxPage оригинала: три примера.
//
// Элементы оригинала — строки и объекты FontItem с шаблоном; здесь это
// ComboBoxItem с подписью, а выбранная строка находится по индексу.

#include "Pages.h"
#include "ShowDialog.h"

#include <algorithm>
#include <cwchar>
#include <iterator>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t inlineHeader[] = {
#include "Snippets/ComboBox/Inline.html.embed"
};
constexpr char8_t inlineCode[] = {
#include "Snippets/ComboBox/Inline.h.embed"
};

FrameworkElement inlineExample() {
#include "Snippets/ComboBox/Inline.h"

    return gallery::controlExample({
        .header = gallery::snippet(inlineHeader),
        .example = StackPanel {combo, output},
        .code = gallery::snippet(inlineCode),
    });
}

constexpr char8_t sourceHeader[] = {
#include "Snippets/ComboBox/ItemsSource.html.embed"
};
constexpr char8_t sourceCode[] = {
#include "Snippets/ComboBox/ItemsSource.h.embed"
};

FrameworkElement sourceExample() {
#include "Snippets/ComboBox/ItemsSource.h"

    return gallery::controlExample({
        .header = gallery::snippet(sourceHeader),
        .example = StackPanel {combo, output},
        .code = gallery::snippet(sourceCode),
    });
}

constexpr char8_t editableHeader[] = {
#include "Snippets/ComboBox/Editable.html.embed"
};
constexpr char8_t editableCode[] = {
#include "Snippets/ComboBox/Editable.h.embed"
};

FrameworkElement editableExample() {
#include "Snippets/ComboBox/Editable.h"

    return gallery::controlExample({
        .header = gallery::snippet(editableHeader),
        .example = StackPanel {combo, output},
        .code = gallery::snippet(editableCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::comboBoxPage() {
    return StackPanel {inlineExample(), sourceExample(), editableExample()};
}