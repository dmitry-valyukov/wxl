// Страница Acrylic -- AcrylicPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t defaultBrushHeader[] = {
#include "Snippets/Acrylic/DefaultAppAcrylicBrush.html.embed"
};
constexpr char8_t defaultBrushCode[] = {
#include "Snippets/Acrylic/DefaultAppAcrylicBrush.h.embed"
};

FrameworkElement defaultBrush() {
#include "Snippets/Acrylic/DefaultAppAcrylicBrush.h"

    return gallery::controlExample({
        .header = gallery::snippet(defaultBrushHeader),
        .example = example,
        .code = gallery::snippet(defaultBrushCode),
    });
}

constexpr char8_t customBrushHeader[] = {
#include "Snippets/Acrylic/CustomAcrylicAppBrush.html.embed"
};
constexpr char8_t customBrushCode[] = {
#include "Snippets/Acrylic/CustomAcrylicAppBrush.h.embed"
};

FrameworkElement customBrush() {
#include "Snippets/Acrylic/CustomAcrylicAppBrush.h"

    return gallery::controlExample({
        .header = gallery::snippet(customBrushHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(customBrushCode),
    });
}

constexpr char8_t luminosityHeader[] = {
#include "Snippets/Acrylic/LuminosityAppAcrylic.html.embed"
};
constexpr char8_t luminosityCode[] = {
#include "Snippets/Acrylic/LuminosityAppAcrylic.h.embed"
};

FrameworkElement luminosity() {
#include "Snippets/Acrylic/LuminosityAppAcrylic.h"

    return gallery::controlExample({
        .header = gallery::snippet(luminosityHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(luminosityCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::acrylicPage() {
    return StackPanel {
        RichTextBlock {
            Paragraph {
                Run {u"Acrylic Brush might fall back to SolidColorbrush in certain scenarios. If you can't see the Acrylic effect, please refer to "},
                Hyperlink {
                    navigateUri = u"https://learn.microsoft.com/windows/apps/design/style/acrylic#usability-and-adaptability",
                    Run {u"Acrylic brush adaptability documentation"},
                },
                Run {u". Acrylic Brush uses in-app acrylic. See "},
                Hyperlink {
                    onClick = [](Object const&, HyperlinkClickEventArgs&) { navigate({Place::Item, L"SystemBackdrops"}); },
                    Run {u"SystemBackdrops (Mica/Acrylic)"},
                },
                Run {u" for background acrylic."},
            },
        },defaultBrush(), customBrush(), luminosity()};
}
