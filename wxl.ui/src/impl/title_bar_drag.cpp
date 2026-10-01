#include "title_bar_drag.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>

namespace wxl::impl {

void set_drag_region(winrt::Microsoft::UI::Xaml::UIElement const& element, core::nullable<bool> const& value) {
    using winrt::Microsoft::UI::Xaml::Controls::TitleBar;
    if (value) {
        TitleBar::SetIsDragRegion(element, *value);
    } else {
        element.ClearValue(TitleBar::IsDragRegionProperty());
    }
}

}  // namespace wxl::impl
