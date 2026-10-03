// Страница TabView -- TabViewPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <algorithm>
#include <string>
#include <vector>
#include "Box.h"
#include "BoundTemplate.h"
#include "NavigationPages.h"
#include "RepeaterData.h"
#include "ResourceBrush.h"
#include "StringList.h"
#include "TabViewWindowing.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Input.h"
#include "generated/Microsoft.UI.Xaml.Media.h"


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t addingClosingHeader[] = {
#include "Snippets/TabView/TabviewSupportAddingClosing.html.embed"
};
constexpr char8_t addingClosingCode[] = {
#include "Snippets/TabView/TabviewSupportAddingClosing.h.embed"
};

FrameworkElement addingClosing() {
#include "Snippets/TabView/TabviewSupportAddingClosing.h"

    return gallery::controlExample({
        .header = gallery::snippet(addingClosingHeader),
        .example = example,
        .code = gallery::snippet(addingClosingCode),
    });
}

constexpr char8_t itemsInMarkupHeader[] = {
#include "Snippets/TabView/TabviewTabviewitemsDefinedMarkup.html.embed"
};
constexpr char8_t itemsInMarkupCode[] = {
#include "Snippets/TabView/TabviewTabviewitemsDefinedMarkup.h.embed"
};

FrameworkElement itemsInMarkup() {
#include "Snippets/TabView/TabviewTabviewitemsDefinedMarkup.h"

    return gallery::controlExample({
        .header = gallery::snippet(itemsInMarkupHeader),
        .example = example,
        .code = gallery::snippet(itemsInMarkupCode),
    });
}

constexpr char8_t boundCollectionHeader[] = {
#include "Snippets/TabView/TabviewBoundCollectionMydata.html.embed"
};
constexpr char8_t boundCollectionCode[] = {
#include "Snippets/TabView/TabviewBoundCollectionMydata.h.embed"
};

FrameworkElement boundCollection() {
#include "Snippets/TabView/TabviewBoundCollectionMydata.h"

    return gallery::controlExample({
        .header = gallery::snippet(boundCollectionHeader),
        .example = example,
        .code = gallery::snippet(boundCollectionCode),
    });
}

constexpr char8_t keyboardingHeader[] = {
#include "Snippets/TabView/TabviewKeyboardingSupport.html.embed"
};
constexpr char8_t keyboardingCode[] = {
#include "Snippets/TabView/TabviewKeyboardingSupport.h.embed"
};

FrameworkElement keyboarding() {
#include "Snippets/TabView/TabviewKeyboardingSupport.h"

    return gallery::controlExample({
        .header = gallery::snippet(keyboardingHeader),
        .example = example,
        .code = gallery::snippet(keyboardingCode),
    });
}

constexpr char8_t customContentHeader[] = {
#include "Snippets/TabView/TabViewYouPutCustomContent.html.embed"
};
constexpr char8_t customContentCode[] = {
#include "Snippets/TabView/TabViewYouPutCustomContent.h.embed"
};

FrameworkElement customContent() {
#include "Snippets/TabView/TabViewYouPutCustomContent.h"

    return gallery::controlExample({
        .header = gallery::snippet(customContentHeader),
        .example = example,
        .code = gallery::snippet(customContentCode),
    });
}

constexpr char8_t tabWidthsHeader[] = {
#include "Snippets/TabView/TabViewTabWidthsEitherBe.html.embed"
};
constexpr char8_t tabWidthsCode[] = {
#include "Snippets/TabView/TabViewTabWidthsEitherBe.h.embed"
};

FrameworkElement tabWidths() {
#include "Snippets/TabView/TabViewTabWidthsEitherBe.h"

    return gallery::controlExample({
        .header = gallery::snippet(tabWidthsHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(tabWidthsCode),
    });
}

constexpr char8_t closeButtonHeader[] = {
#include "Snippets/TabView/TabViewCloseButtonBePersistent.html.embed"
};
constexpr char8_t closeButtonCode[] = {
#include "Snippets/TabView/TabViewCloseButtonBePersistent.h.embed"
};

FrameworkElement closeButton() {
#include "Snippets/TabView/TabViewCloseButtonBePersistent.h"

    return gallery::controlExample({
        .header = gallery::snippet(closeButtonHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(closeButtonCode),
    });
}

constexpr char8_t colorIconsHeader[] = {
#include "Snippets/TabView/TabviewColorTabIcons.html.embed"
};
constexpr char8_t colorIconsCode[] = {
#include "Snippets/TabView/TabviewColorTabIcons.h.embed"
};

FrameworkElement colorIcons() {
#include "Snippets/TabView/TabviewColorTabIcons.h"

    return gallery::controlExample({
        .header = gallery::snippet(colorIconsHeader),
        .example = example,
        .code = gallery::snippet(colorIconsCode),
    });
}

constexpr char8_t accentStripHeader[] = {
#include "Snippets/TabView/TabviewAccentColoredTabstrip.html.embed"
};
constexpr char8_t accentStripCode[] = {
#include "Snippets/TabView/TabviewAccentColoredTabstrip.h.embed"
};

FrameworkElement accentStrip() {
#include "Snippets/TabView/TabviewAccentColoredTabstrip.h"

    return gallery::controlExample({
        .header = gallery::snippet(accentStripHeader),
        .example = example,
        .code = gallery::snippet(accentStripCode),
    });
}

constexpr char8_t windowingHeader[] = {
#include "Snippets/TabView/CompleteTabviewWindowingSample.html.embed"
};
constexpr char8_t windowingCode[] = {
#include "Snippets/TabView/CompleteTabviewWindowingSample.h.embed"
};

FrameworkElement windowing() {
#include "Snippets/TabView/CompleteTabviewWindowingSample.h"

    return gallery::controlExample({
        .header = gallery::snippet(windowingHeader),
        .example = example,
        .code = gallery::snippet(windowingCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::tabViewPage() {
    return StackPanel {addingClosing(), itemsInMarkup(), boundCollection(), keyboarding(), customContent(), tabWidths(), closeButton(), colorIcons(), accentStrip(), windowing()};
}
