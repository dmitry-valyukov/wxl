#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <wxl/Microsoft.UI.Xaml.Automation.Peers.Enums.h>
#include "../hstring_param.h"

// What stands behind `automationName = u"..."`, `automationId = u"..."`,
// `automationLiveSetting = AutomationLiveSetting::Polite` and the other automation tags on any
// element: WinUI's AutomationProperties, its setters by their own names. The id is what a test or a
// screen reader's script finds the element by; the live setting says that a change of the
// element's text is to be announced (Polite: when the reader is idle, Assertive: at once).
//
// Private: the element arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

namespace wxl {
class UIElement;
}

namespace wxl::impl {

class automation_properties {
public:
    automation_properties() = delete;

    /// The name a screen reader speaks, and nothing else -- the half of toolTip that carries no
    /// hover hint. A Button's accessible name comes from its content only when that content is a
    /// string, so a list row built out of a panel is nameless to automation however much text it
    /// shows; a hover hint there would only repeat what is on screen, so this is a property of its
    /// own rather than a use of toolTip.
    static void set_name(winrt::Microsoft::UI::Xaml::UIElement const& element,
                         hstring_param const& name);

    static void set_automation_id(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                  hstring_param const& id);

    static void set_live_setting(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                 AutomationLiveSetting setting);

    static void set_accessibility_view(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                       AccessibilityView view);

    static void set_heading_level(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                  AutomationHeadingLevel level);

    static void set_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                  AutomationLandmarkType type);

    static void set_automation_control_type(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                            AutomationControlType type);

    static void set_localized_control_type(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                           hstring_param const& text);

    static void set_localized_landmark_type(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                            hstring_param const& text);

    static void set_accelerator_key(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                    hstring_param const& keys);

    static void set_help_text(winrt::Microsoft::UI::Xaml::UIElement const& element,
                              hstring_param const& text);

    static void set_full_description(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                     hstring_param const& text);

    static void set_position_in_set(winrt::Microsoft::UI::Xaml::UIElement const& element,
                                    int32_t position);

    static void set_size_of_set(winrt::Microsoft::UI::Xaml::UIElement const& element, int32_t size);

    static void set_labeled_by(winrt::Microsoft::UI::Xaml::UIElement const& element,
                               UIElement const& label);
};

}  // namespace wxl::impl
