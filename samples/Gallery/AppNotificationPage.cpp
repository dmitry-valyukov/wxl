// Страница AppNotification -- AppNotificationPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include "ApplicationFolder.h"
#include "generated/Microsoft.Windows.AppNotifications.h"
#include "generated/Microsoft.Windows.AppNotifications.Builder.h"

using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t basicHeader[] = {
#include "Snippets/AppNotification/AppNotificationBasicNotification.html.embed"
};
constexpr char8_t basicCode[] = {
#include "Snippets/AppNotification/AppNotificationBasicNotification.h.embed"
};

FrameworkElement basic() {
#include "Snippets/AppNotification/AppNotificationBasicNotification.h"

    return gallery::controlExample({
        .header = gallery::snippet(basicHeader),
        .example = example,
        .code = gallery::snippet(basicCode),
    });
}

constexpr char8_t logoHeader[] = {
#include "Snippets/AppNotification/AppNotificationInformationalNotificationLogoCustom.html.embed"
};
constexpr char8_t logoCode[] = {
#include "Snippets/AppNotification/AppNotificationInformationalNotificationLogoCustom.h.embed"
};

FrameworkElement customLogo() {
#include "Snippets/AppNotification/AppNotificationInformationalNotificationLogoCustom.h"

    return gallery::controlExample({
        .header = gallery::snippet(logoHeader),
        .example = example,
        .options = {options},
        .code = gallery::snippet(logoCode),
    });
}

constexpr char8_t heroHeader[] = {
#include "Snippets/AppNotification/AppNotificationVisualNotificationHeroImage.html.embed"
};
constexpr char8_t heroCode[] = {
#include "Snippets/AppNotification/AppNotificationVisualNotificationHeroImage.h.embed"
};

FrameworkElement hero() {
#include "Snippets/AppNotification/AppNotificationVisualNotificationHeroImage.h"

    return gallery::controlExample({
        .header = gallery::snippet(heroHeader),
        .example = example,
        .code = gallery::snippet(heroCode),
    });
}

constexpr char8_t controlsHeader[] = {
#include "Snippets/AppNotification/NotificationAppnotificationControls.html.embed"
};
constexpr char8_t controlsCode[] = {
#include "Snippets/AppNotification/NotificationAppnotificationControls.h.embed"
};

FrameworkElement controls() {
#include "Snippets/AppNotification/NotificationAppnotificationControls.h"

    return gallery::controlExample({
        .header = gallery::snippet(controlsHeader),
        .example = example,
        .code = gallery::snippet(controlsCode),
    });
}

constexpr char8_t progressHeader[] = {
#include "Snippets/AppNotification/AppNotificationNotificationProgressbar.html.embed"
};
constexpr char8_t progressCode[] = {
#include "Snippets/AppNotification/AppNotificationNotificationProgressbar.h.embed"
};

FrameworkElement progress() {
#include "Snippets/AppNotification/AppNotificationNotificationProgressbar.h"

    return gallery::controlExample({
        .header = gallery::snippet(progressHeader),
        .example = example,
        .code = gallery::snippet(progressCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::appNotificationPage() {
    return StackPanel {basic(), customLogo(), hero(), controls(), progress()};
}
