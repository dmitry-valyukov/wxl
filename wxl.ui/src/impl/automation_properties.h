#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../generated/Microsoft.UI.Xaml.Automation.Peers.Enums.h"
#include "../hstring_param.h"

// What stands behind `automationId = u"..."` and `automationLiveSetting = AutomationLiveSetting::Polite` on any
// element -- AutomationProperties.AutomationId and AutomationProperties.LiveSetting. The id is what a test or a
// screen reader's script finds the element by; the live setting says that a change of the element's text is to be
// announced (Polite: when the reader is idle, Assertive: at once).
//
// Private: the element arrives as the projection type the wrapper already holds, so this header is one only wxl's
// own sources ever include.

namespace wxl {
class UIElement;
}

namespace wxl::impl {

void set_automation_id(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& id);

void set_automation_live_setting(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationLiveSetting setting);

void set_automation_accessibility_view(winrt::Microsoft::UI::Xaml::UIElement const& element, AccessibilityView view);

void set_automation_heading_level(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationHeadingLevel level);

void set_automation_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element, AutomationLandmarkType type);

void set_automation_localized_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text);

void set_automation_accelerator_key(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& keys);

void set_automation_help_text(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text);

void set_automation_full_description(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text);

void set_automation_position_in_set(winrt::Microsoft::UI::Xaml::UIElement const& element, int32_t position);

void set_automation_size_of_set(winrt::Microsoft::UI::Xaml::UIElement const& element, int32_t size);

void set_automation_labeled_by(winrt::Microsoft::UI::Xaml::UIElement const& element, UIElement const& label);
}  // namespace wxl::impl
