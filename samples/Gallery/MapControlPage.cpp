// Страница MapControl -- MapControlPage оригинала.

#include "Pages.h"
#include "Shell.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Documents.h>
#include <wxl/Microsoft.UI.Xaml.Input.h>
#include <wxl/Microsoft.UI.Xaml.Automation.Peers.Enums.h>
#include <wxl/Windows.Devices.Geolocation.h>
#include <wxl/Windows.Devices.Geolocation.Structs.h>


using namespace wxl;
using namespace wxl::dsl;

namespace {

constexpr char8_t pinHeader[] = {
#include "Snippets/MapControl/MapControlShowingPinMap.html.embed"
};
constexpr char8_t pinCode[] = {
#include "Snippets/MapControl/MapControlShowingPinMap.h.embed"
};

FrameworkElement pin() {
#include "Snippets/MapControl/MapControlShowingPinMap.h"

    return gallery::controlExample({
        .header = gallery::snippet(pinHeader),
        .example = example,
        .code = gallery::snippet(pinCode),
    });
}

}  // namespace

wxl::FrameworkElement gallery::mapControlPage() {
    return StackPanel {TextBlock {Margin {0, 0, 0, 12}, textWrapping = TextWrapping::Wrap, Run {u"Follow instructions "}, Hyperlink {navigateUri = u"https://learn.microsoft.com/azure/azure-maps/how-to-manage-account-keys", Run {u"here"}}, Run {u" to obtain your MapServiceToken."}}, Image {height = 320, hAlign.left, automationAccessibilityView = AccessibilityView::Raw, source = u"Assets/SampleMedia/MapExample.png"}, pin()};
}
