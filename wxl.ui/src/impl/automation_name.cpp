#include "automation_name.h"

#include <winrt/Microsoft.UI.Xaml.Automation.h>

namespace wxl::impl {

void set_automation_name(winrt::Microsoft::UI::Xaml::UIElement const& element,
                         string_param name) {
    // Still parsed and handed on in wchar_t inside wxl; the unit changes at
    // the boundary above.
    std::wstring_view const text = name.wide();
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element,
                                                                          winrt::hstring{text});
}

}  // namespace wxl::impl
