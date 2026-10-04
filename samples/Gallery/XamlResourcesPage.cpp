// Страница XamlResources — XamlResourcesPage оригинала. Ресурсы трёх уровней, StaticResource и ThemeResource —
// у wxl это значения, resourceBrush и пути `brushes`; собственный словарь тем остаётся разметкой, и пример читает её loadXaml.

#include "Pages.h"
#include "Shell.h"
#include "LoadXaml.h"
#include "ResourceBrush.h"

#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Documents.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t levelsHeader[] = {
#include "Snippets/XamlResources/Xamlresources.html.embed"
};
constexpr char8_t levelsCode[] = {
#include "Snippets/XamlResources/Xamlresources.h.embed"
};
constexpr char8_t versusHeader[] = {
#include "Snippets/XamlResources/XamlResourcesStaticresourceVersusThemeresource.html.embed"
};
constexpr char8_t versusCode[] = {
#include "Snippets/XamlResources/XamlResourcesStaticresourceVersusThemeresource.h.embed"
};
constexpr char8_t themeHeader[] = {
#include "Snippets/XamlResources/XamlResourcesDefineNewThemeResource.html.embed"
};
constexpr char8_t themeCode[] = {
#include "Snippets/XamlResources/XamlResourcesDefineNewThemeResource.h.embed"
};

FrameworkElement levels() {
#include "Snippets/XamlResources/Xamlresources.h"
    return gallery::controlExample({.header = gallery::snippet(levelsHeader), .example = example, .code = gallery::snippet(levelsCode)});
}

FrameworkElement versus() {
#include "Snippets/XamlResources/XamlResourcesStaticresourceVersusThemeresource.h"
    return gallery::controlExample({.header = gallery::snippet(versusHeader), .example = example, .code = gallery::snippet(versusCode)});
}

FrameworkElement newTheme() {
#include "Snippets/XamlResources/XamlResourcesDefineNewThemeResource.h"
    return gallery::controlExample({.header = gallery::snippet(themeHeader), .example = example, .code = gallery::snippet(themeCode)});
}

}  // namespace

FrameworkElement gallery::xamlResourcesPage() {
    return StackPanel {
        TextBlock {Margin {0, 12, 0, 4}, styles.TextBlock.Subtitle, u"Creating and using resources"},
        StackPanel {
            spacing = 12.0,
            RichTextBlock {Paragraph {Run {u"A resource is a value under a key: the key is a unique identifier, the value a color, a brush, a text. "
                                           u"In XAML it is an entry of a "},
                                      Run {fontFamily = u"Consolas", u"ResourceDictionary"}, Run {u"; here it is a value of the program."}}},
            RichTextBlock {
                Paragraph {Run {u"• App-level: resources of the framework and the application, read by key (resourceColor, resourceBrush)."}},
                Paragraph {Run {u"• Page-level: values of the function that builds the page."}},
                Paragraph {Run {u"• Control-level: a dictionary of the control itself (dsl::resources)."}},
            },
            RichTextBlock {
                Paragraph {fontWeight = FontWeight {600}, Run {u"Tips"}},
                Paragraph {Run {u"• Naming: descriptive keys should always be used for resources to make them easier to identify."}},
                Paragraph {Run {u"• Scope: values should be defined at the narrowest scope possible to improve maintainability."}},
            },
        },
        levels(),
        TextBlock {Margin {0, 24, 0, 4}, styles.TextBlock.Subtitle, u"Theme resources"},
        StackPanel {
            spacing = 12.0,
            RichTextBlock {Paragraph {Run {u"WinUI 3 includes built-in theme resources for commonly used colors. See all brushes on the "},
                                      Hyperlink {onClick = [](auto&&...) { gallery::navigate({gallery::Place::Item, L"Color"}); }, Run {u"Color page"}},
                                      Run {u"."}}},
            RichTextBlock {
                Paragraph {Run {u"• A path of "}, Run {fontWeight = FontWeight {600}, u"brushes"},
                           Run {u" is looked up again when the theme of the element changes, as {ThemeResource} is."}},
                Paragraph {Run {u"• "}, Run {fontWeight = FontWeight {600}, u"ThemeDictionaries"},
                           Run {u" give different values for light and dark themes; they are markup."}},
            },
        },
        versus(),
        newTheme(),
    };
}
