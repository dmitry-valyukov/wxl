// Страница BadgeNotificationManager -- BadgeNotificationManagerPage оригинала.

#include "Pages.h"
#include "Packaged.h"
#include "generated/Microsoft.Windows.BadgeNotifications.h"
#include "Shell.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t countHeader[] = {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsCount.html.embed"
};
constexpr char8_t countCode[] = {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsCount.h.embed"
};

FrameworkElement count() {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsCount.h"

    return gallery::controlExample({
        .header = gallery::snippet(countHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(countCode),
    });
}

constexpr char8_t glyphExampleHeader[] = {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsGlyph.html.embed"
};
constexpr char8_t glyphExampleCode[] = {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsGlyph.h.embed"
};

FrameworkElement glyphExample() {
#include "Snippets/BadgeNotificationManager/BadgeNotificationManagerSettingBadgeNotificationsGlyph.h"

    return gallery::controlExample({
        .header = gallery::snippet(glyphExampleHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(glyphExampleCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::badgeNotificationManagerPage() {
    return StackPanel {
        InfoBar {
            Margin {0, 10, 0, 0},
            isOpen = true,
            isClosable = false,
            severity = InfoBarSeverity::Warning,
            title = u"BadgeNotificationManager is not available in unpackaged mode.",
            message = u"This API requires the app to be running in packaged mode (MSIX).",
        },count(), glyphExample()};
}
