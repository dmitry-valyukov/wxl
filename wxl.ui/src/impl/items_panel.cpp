#include "items_panel.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>

namespace wxl::impl {

void set_items_panel_orientation(winrt::Microsoft::UI::Xaml::Controls::ItemsControl const& control, Orientation orientation) {
    namespace xaml = winrt::Microsoft::UI::Xaml;

    // Both the text and the answer of the loader are short-lived: the control keeps the template.
    winrt::hstring const text = orientation == Orientation::Vertical
        ? L"<ItemsPanelTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>"
          L"<VirtualizingStackPanel Orientation='Vertical'/></ItemsPanelTemplate>"
        : L"<ItemsPanelTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>"
          L"<VirtualizingStackPanel Orientation='Horizontal'/></ItemsPanelTemplate>";
    control.ItemsPanel(xaml::Markup::XamlReader::Load(text).as<xaml::Controls::ItemsPanelTemplate>());
}

}  // namespace wxl::impl
