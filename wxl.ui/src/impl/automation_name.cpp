#include "automation_name.h"

#include "conversions.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace wxl::impl {

void set_automation_name(winrt::Microsoft::UI::Xaml::UIElement const& element,
                         hstring_param const& name) {
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element, to_winrt(name));
}

}  // namespace wxl::impl
