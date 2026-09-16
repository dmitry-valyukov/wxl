#include "tool_tip.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

void set_tool_tip(winrt::Microsoft::UI::Xaml::UIElement const& element, string_param name) {
    std::wstring_view const text = name.wide();
    winrt::hstring const value{text};
    winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(element,
                                                                     winrt::box_value(value));
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element, value);
}

}  // namespace wxl::impl
