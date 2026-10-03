#include "automation_properties.h"

#include "conversions.h"

#include <winrt/Microsoft.UI.Xaml.Automation.Peers.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace wxl::impl {

namespace automation = winrt::Microsoft::UI::Xaml::Automation;

void set_automation_id(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& id) {
    automation::AutomationProperties::SetAutomationId(element, to_winrt(id));
}

void set_automation_live_setting(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationLiveSetting setting) {
    automation::AutomationProperties::SetLiveSetting(element, static_cast<automation::Peers::AutomationLiveSetting>(setting));
}

void set_automation_accessibility_view(winrt::Microsoft::UI::Xaml::UIElement const& element, AccessibilityView view) {
    automation::AutomationProperties::SetAccessibilityView(element, static_cast<automation::Peers::AccessibilityView>(view));
}

}  // namespace wxl::impl
