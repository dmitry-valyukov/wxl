#include "tool_tip.h"

#include "conversions.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

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

void set_tool_tip_element(winrt::Microsoft::UI::Xaml::UIElement const& element,
                          ToolTip const& tip) {
    winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(
        element, *Object::Impl::get_typed<ToolTip>(tip));
}

}  // namespace wxl::impl
