// Страница Bound collection — своя у wxl, рядом с Binding: списочный контрол привязан к списку модели
// (core::observable_list) одним значением, itemsSource = BindOutput {list, build}. Список растёт кнопкой, строка элемента
// меняется полем самого элемента, фильтр пересобирает список целиком.

#include "Pages.h"
#include "Shell.h"
#include "Bind.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>

import wxl.fmt;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t growingHeader[] = {
#include "Snippets/BoundCollection/BoundCollectionGrowing.html.embed"
};
constexpr char8_t growingCode[] = {
#include "Snippets/BoundCollection/BoundCollectionGrowing.h.embed"
};

FrameworkElement growingExample() {
#include "Snippets/BoundCollection/BoundCollectionGrowing.h"

    return gallery::controlExample({.header = gallery::snippet(growingHeader), .example = example, .code = gallery::snippet(growingCode)});
}

constexpr char8_t itemFieldHeader[] = {
#include "Snippets/BoundCollection/BoundCollectionItemField.html.embed"
};
constexpr char8_t itemFieldCode[] = {
#include "Snippets/BoundCollection/BoundCollectionItemField.h.embed"
};

FrameworkElement itemFieldExample() {
#include "Snippets/BoundCollection/BoundCollectionItemField.h"

    return gallery::controlExample({.header = gallery::snippet(itemFieldHeader), .example = example, .code = gallery::snippet(itemFieldCode)});
}

constexpr char8_t filterHeader[] = {
#include "Snippets/BoundCollection/BoundCollectionFilter.html.embed"
};
constexpr char8_t filterCode[] = {
#include "Snippets/BoundCollection/BoundCollectionFilter.h.embed"
};

FrameworkElement filterExample() {
#include "Snippets/BoundCollection/BoundCollectionFilter.h"

    return gallery::controlExample({.header = gallery::snippet(filterHeader), .example = example, .code = gallery::snippet(filterCode)});
}

}  // namespace

FrameworkElement gallery::boundCollectionPage() {
    return StackPanel {
        spacing = 12.0,
        RichTextBlock {
            Paragraph {fontWeight = FontWeight {600}, Run {u"Key concepts"}},
            Paragraph {Run {u"• Source: a list of a model, core::observable_list. Every change says what happened and where: "
                            u"items inserted, erased, replaced, or the whole list anew."}},
            Paragraph {Run {u"• Target: the items of a ListView, a GridView, an ItemsView or an ItemsRepeater, with the function that "
                            u"builds the element of an item: itemsSource = BindOutput {list, build}. The type of the items is checked "
                            u"against the function where the binding is written."}},
            Paragraph {Run {u"• The control builds the elements in view only, and each change of the list reaches it as that change."}},
            Paragraph {Run {u"• An element may bind to the fields of its item; those bindings go when the control gives the element back."}},
            Paragraph {Run {u"• boundItem(list, item) is the item of the model behind what the control hands out (clickedItem, "
                            u"selectedItem)."}},
        },
        growingExample(),
        itemFieldExample(),
        filterExample(),
    };
}
