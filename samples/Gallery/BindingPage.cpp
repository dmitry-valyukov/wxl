// Страница Binding — BindingPage оригинала. Привязка у wxl своя: источник — поле модели (core::observable), цель — свойство
// контрола, направление выбирает написанное слово: BindOutput, BindInput или Bind. Конвертер XAML — функция рядом с привязкой.

#include "Pages.h"
#include "Shell.h"
#include "Bind.h"
#include "Box.h"
#include "ItemBuilder.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>

import wxl.fmt;

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlsHeader[] = {
#include "Snippets/Binding/BindingControls.html.embed"
};
constexpr char8_t controlsCode[] = {
#include "Snippets/Binding/BindingControls.h.embed"
};

FrameworkElement controlsExample() {
#include "Snippets/Binding/BindingControls.h"

    return gallery::controlExample({.header = gallery::snippet(controlsHeader), .example = example, .code = gallery::snippet(controlsCode)});
}

constexpr char8_t propertyFieldHeader[] = {
#include "Snippets/Binding/BindingPropertyCodeBehind.html.embed"
};
constexpr char8_t propertyFieldCode[] = {
#include "Snippets/Binding/BindingPropertyCodeBehind.h.embed"
};

FrameworkElement propertyFieldExample() {
#include "Snippets/Binding/BindingPropertyCodeBehind.h"

    return gallery::controlExample({.header = gallery::snippet(propertyFieldHeader), .example = example, .code = gallery::snippet(propertyFieldCode)});
}

constexpr char8_t functionOfFieldsHeader[] = {
#include "Snippets/Binding/BindingFunctionBind.html.embed"
};
constexpr char8_t functionOfFieldsCode[] = {
#include "Snippets/Binding/BindingFunctionBind.h.embed"
};

FrameworkElement functionOfFieldsExample() {
#include "Snippets/Binding/BindingFunctionBind.h"

    return gallery::controlExample({.header = gallery::snippet(functionOfFieldsHeader), .example = example, .code = gallery::snippet(functionOfFieldsCode)});
}

constexpr char8_t converterHeader[] = {
#include "Snippets/Binding/ConverterBinding.html.embed"
};
constexpr char8_t converterCode[] = {
#include "Snippets/Binding/ConverterBinding.h.embed"
};

FrameworkElement converterExample() {
#include "Snippets/Binding/ConverterBinding.h"

    return gallery::controlExample({.header = gallery::snippet(converterHeader), .example = example, .code = gallery::snippet(converterCode)});
}

constexpr char8_t viewModelHeader[] = {
#include "Snippets/Binding/BindingViewModel.html.embed"
};
constexpr char8_t viewModelCode[] = {
#include "Snippets/Binding/BindingViewModel.h.embed"
};

FrameworkElement viewModelExample() {
#include "Snippets/Binding/BindingViewModel.h"

    return gallery::controlExample({.header = gallery::snippet(viewModelHeader), .example = example, .code = gallery::snippet(viewModelCode)});
}

constexpr char8_t nullValueHeader[] = {
#include "Snippets/Binding/BindingTargetnullvalue.html.embed"
};
constexpr char8_t nullValueCode[] = {
#include "Snippets/Binding/BindingTargetnullvalue.h.embed"
};

FrameworkElement nullValueExample() {
#include "Snippets/Binding/BindingTargetnullvalue.h"

    return gallery::controlExample({.header = gallery::snippet(nullValueHeader), .example = example, .code = gallery::snippet(nullValueCode)});
}

constexpr char8_t masterDetailHeader[] = {
#include "Snippets/Binding/BindingCollectionDataTemplates.html.embed"
};
constexpr char8_t masterDetailCode[] = {
#include "Snippets/Binding/BindingCollectionDataTemplates.h.embed"
};

FrameworkElement masterDetailExample() {
#include "Snippets/Binding/BindingCollectionDataTemplates.h"

    return gallery::controlExample({.header = gallery::snippet(masterDetailHeader), .example = example, .code = gallery::snippet(masterDetailCode)});
}

}  // namespace

FrameworkElement gallery::bindingPage() {
    return StackPanel {
        spacing = 12.0,
        RichTextBlock {
            Paragraph {fontWeight = FontWeight {600}, Run {u"Key concepts"}},
            Paragraph {Run {u"\u2022 Target: the property of a control to which data is bound (Text, Background, Visibility)."}},
            Paragraph {Run {u"\u2022 Source: a field of a model, an observable. It is held by address and never owned by the binding: the model "
                            u"outlives the description, and the binding is a watch inside the field."}},
            Paragraph {Run {u"\u2022 Direction is the word that is written:"}},
            Paragraph {Run {u"    \u25E6 BindOutput updates the target when the source changes (OneWay)."}},
            Paragraph {Run {u"    \u25E6 Bind updates both the target and the source (TwoWay)."}},
            Paragraph {Run {u"    \u25E6 BindInput updates the source from the target alone."}},
            Paragraph {Run {u"    \u25E6 A plain value sets the target once (OneTime)."}},
        },
        RichTextBlock {Paragraph {Run {u"The Quadratic sample of wxl is built on this: three fields in, a field that follows them, answers out."}}},
        controlsExample(),
        propertyFieldExample(),
        functionOfFieldsExample(),
        converterExample(),
        viewModelExample(),
        nullValueExample(),
        masterDetailExample(),
    };
}
