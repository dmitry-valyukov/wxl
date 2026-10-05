#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "Announce.h"
#include "Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/conversions.h"

namespace wxl {

namespace peers = winrt::Microsoft::UI::Xaml::Automation::Peers;

namespace {

void raise(UIElement const& element, peers::AutomationNotificationKind kind, hstring_param const& text, hstring_param const& activityId) {
    auto const native = Object::Impl::as<winrt::Microsoft::UI::Xaml::UIElement>(element);
    if (auto const peer = peers::FrameworkElementAutomationPeer::FromElement(native)) {
        peer.RaiseNotificationEvent(kind, peers::AutomationNotificationProcessing::ImportantMostRecent, impl::to_winrt(text),
                                    impl::to_winrt(activityId));
    }
}

}  // namespace

void announce(UIElement const& element, hstring_param const& text, hstring_param const& activityId) {
    raise(element, peers::AutomationNotificationKind::ActionCompleted, text, activityId);
}

void announceOther(UIElement const& element, hstring_param const& text, hstring_param const& activityId) {
    raise(element, peers::AutomationNotificationKind::Other, text, activityId);
}

}  // namespace wxl
