// Страница Templates — TemplatesPage оригинала и то, что у wxl на этом месте: ControlTemplate и ItemsPanelTemplate
// остаются разметкой (loadXaml), DataTemplate — функция от элемента, а описание строится из Preset и Template,
// вложенных друг в друга и в настоящие описания.

#include "Pages.h"
#include "LoadXaml.h"
#include "Box.h"
#include "ItemBuilder.h"
#include "StringList.h"

#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>
#include <wxl/Microsoft.UI.Xaml.Media.h>

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t controlHeader[] = {
#include "Snippets/Templates/TemplatesCustomizeLookTextboxControltemplate.html.embed"
};
constexpr char8_t controlCode[] = {
#include "Snippets/Templates/TemplatesCustomizeLookTextboxControltemplate.h.embed"
};
constexpr char8_t dataHeader[] = {
#include "Snippets/Templates/TemplatesCustomizeComboboxItemtemplateDatatemplate.html.embed"
};
constexpr char8_t dataCode[] = {
#include "Snippets/Templates/TemplatesCustomizeComboboxItemtemplateDatatemplate.h.embed"
};
constexpr char8_t panelHeader[] = {
#include "Snippets/Templates/TemplatesCustomizeItemscontrolItemspaneltemplate.html.embed"
};
constexpr char8_t panelCode[] = {
#include "Snippets/Templates/TemplatesCustomizeItemscontrolItemspaneltemplate.h.embed"
};
constexpr char8_t presetsHeader[] = {
#include "Snippets/Templates/TemplatesPresetsNested.html.embed"
};
constexpr char8_t presetsCode[] = {
#include "Snippets/Templates/TemplatesPresetsNested.h.embed"
};
constexpr char8_t templatesHeader[] = {
#include "Snippets/Templates/TemplatesBuiltWhereApplied.html.embed"
};
constexpr char8_t templatesCode[] = {
#include "Snippets/Templates/TemplatesBuiltWhereApplied.h.embed"
};

FrameworkElement controlTemplate() {
#include "Snippets/Templates/TemplatesCustomizeLookTextboxControltemplate.h"
    return gallery::controlExample({.header = gallery::snippet(controlHeader), .example = example, .code = gallery::snippet(controlCode)});
}

FrameworkElement dataTemplate() {
#include "Snippets/Templates/TemplatesCustomizeComboboxItemtemplateDatatemplate.h"
    return gallery::controlExample({.header = gallery::snippet(dataHeader), .example = example, .code = gallery::snippet(dataCode)});
}

FrameworkElement itemsPanelTemplate() {
#include "Snippets/Templates/TemplatesCustomizeItemscontrolItemspaneltemplate.h"
    return gallery::controlExample({.header = gallery::snippet(panelHeader), .example = example, .code = gallery::snippet(panelCode)});
}

FrameworkElement nestedPresets() {
#include "Snippets/Templates/TemplatesPresetsNested.h"
    return gallery::controlExample({.header = gallery::snippet(presetsHeader), .example = example, .code = gallery::snippet(presetsCode)});
}

FrameworkElement builtTemplates() {
#include "Snippets/Templates/TemplatesBuiltWhereApplied.h"
    return gallery::controlExample({.header = gallery::snippet(templatesHeader), .example = example, .code = gallery::snippet(templatesCode)});
}

}  // namespace

FrameworkElement gallery::templatesPage() {
    return StackPanel {
        spacing = 12.0,
        Margin {0, 12, 0, 0},
        RichTextBlock {
            Paragraph {fontWeight = FontWeight {600}, Run {u"Templates of XAML"}},
            Paragraph {Run {u"There are 3 types of templates in XAML:"}},
            Paragraph {Run {u"• ControlTemplate: customizes the structure of a control. It names the parts of the control, so it is markup."}},
            Paragraph {Run {u"• DataTemplate: changes how individual items are displayed in a control like a ComboBox or ListView. "
                            u"Here it is a function from the item to an element."}},
            Paragraph {Run {u"• ItemsPanelTemplate: defines how a collection of items is laid out. Markup as well."}},
        },
        controlTemplate(),
        dataTemplate(),
        itemsPanelTemplate(),
        TextBlock {Margin {0, 24, 0, 0}, styles.TextBlock.Subtitle, u"Presets and templates of wxl"},
        RichTextBlock {
            Paragraph {Run {u"What a style and a template of XAML are for -- giving a look a name and using it again -- wxl does with two "
                            u"types, and they differ in what they keep."}},
            Paragraph {Bold {Run {u"Preset"}}, Run {u" keeps its arguments: the properties, unnamed values, handlers and children written in "
                                                  u"its braces, as they were written. It is worn by whatever has those properties, either "
                                                  u"written unnamed inside braces or applied to an object that exists (Apply)."}},
            Paragraph {Bold {Run {u"Template<T>"}}, Run {u" keeps its arguments too, but builds an object of T where it is applied, and builds a "
                                                       u"new one every time. A live object -- a child, a brush -- written in a preset would be made "
                                                       u"once and handed to the framework twice; a template is the way to write it."}},
            Paragraph {Run {u"Both stand wherever an argument stands, so they nest: presets in presets, presets in templates, templates in "
                            u"templates, templates in presets, and each in a real description."}},
        },
        nestedPresets(),
        builtTemplates(),
    };
}
