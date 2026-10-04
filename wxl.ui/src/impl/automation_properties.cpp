#include "automation_properties.h"

#include "conversions.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>

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

void set_automation_heading_level(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationHeadingLevel level) {
    automation::AutomationProperties::SetHeadingLevel(element, static_cast<automation::Peers::AutomationHeadingLevel>(level));
}

void set_automation_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationLandmarkType type) {
    automation::AutomationProperties::SetLandmarkType(element, static_cast<automation::Peers::AutomationLandmarkType>(type));
}

void set_automation_control_type(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationControlType type) {
    automation::AutomationProperties::SetAutomationControlType(element, static_cast<automation::Peers::AutomationControlType>(type));
}

void set_automation_localized_control_type(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text) {
    automation::AutomationProperties::SetLocalizedControlType(element, to_winrt(text));
}
void set_automation_localized_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text) {
    automation::AutomationProperties::SetLocalizedLandmarkType(element, to_winrt(text));
}

void set_automation_accelerator_key(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& keys) {
    automation::AutomationProperties::SetAcceleratorKey(element, to_winrt(keys));
}

void set_automation_help_text(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text) {
    automation::AutomationProperties::SetHelpText(element, to_winrt(text));
}

void set_automation_full_description(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text) {
    automation::AutomationProperties::SetFullDescription(element, to_winrt(text));
}

void set_automation_position_in_set(winrt::Microsoft::UI::Xaml::UIElement const& element, int32_t position) {
    automation::AutomationProperties::SetPositionInSet(element, position);
}

void set_automation_size_of_set(winrt::Microsoft::UI::Xaml::UIElement const& element, int32_t size) {
    automation::AutomationProperties::SetSizeOfSet(element, size);
}

void set_automation_labeled_by(winrt::Microsoft::UI::Xaml::UIElement const& element, UIElement const& label) {
    automation::AutomationProperties::SetLabeledBy(element, *Object::Impl::get_typed<UIElement>(label));
}
}  // namespace wxl::impl
