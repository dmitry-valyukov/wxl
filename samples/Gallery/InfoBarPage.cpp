// Страница InfoBar -- InfoBarPage оригинала.

#include "Pages.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t changeHeader[] = {
#include "Snippets/InfoBar/ClosableInfoBarOptionsChange.html.embed"
};
constexpr char8_t changeCode[] = {
#include "Snippets/InfoBar/ClosableInfoBarOptionsChange.h.embed"
};

FrameworkElement change() {
#include "Snippets/InfoBar/ClosableInfoBarOptionsChange.h"

    return gallery::controlExample({
        .header = gallery::snippet(changeHeader),
        .example = bar,
        .options = {StackPanel {width = 150, isOpen, severity}},
        .code = gallery::snippet(changeCode),
    });
}

constexpr char8_t longShortHeader[] = {
#include "Snippets/InfoBar/ClosableInfoBarLongShort.html.embed"
};
constexpr char8_t longShortCode[] = {
#include "Snippets/InfoBar/ClosableInfoBarLongShort.h.embed"
};

FrameworkElement longShort() {
#include "Snippets/InfoBar/ClosableInfoBarLongShort.h"

    return gallery::controlExample({
        .header = gallery::snippet(longShortHeader),
        .example = bar,
        .options = {StackPanel {width = 150, isOpen, length, actionButton}},
        .code = gallery::snippet(longShortCode),
    });
}

constexpr char8_t displayHeader[] = {
#include "Snippets/InfoBar/ClosableInfoBarOptionsDisplay.html.embed"
};
constexpr char8_t displayCode[] = {
#include "Snippets/InfoBar/ClosableInfoBarOptionsDisplay.h.embed"
};

FrameworkElement display() {
#include "Snippets/InfoBar/ClosableInfoBarOptionsDisplay.h"

    return gallery::controlExample({
        .header = gallery::snippet(displayHeader),
        .example = bar,
        .options = {StackPanel {width = 150, isOpen, isIconVisible, isClosable}},
        .code = gallery::snippet(displayCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::infoBarPage() {
    return StackPanel {change(), longShort(), display()};
}
