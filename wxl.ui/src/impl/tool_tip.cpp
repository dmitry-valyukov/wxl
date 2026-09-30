#include "tool_tip.h"

#include "conversions.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

void set_tool_tip(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& name) {
    winrt::hstring const& value = to_winrt(name);
    winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(element,
                                                                     winrt::box_value(value));
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element, value);
}

}  // namespace wxl::impl
