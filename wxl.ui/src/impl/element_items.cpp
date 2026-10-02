#include "element_items.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>

namespace wxl::impl {

void set_element_items(winrt::Microsoft::UI::Xaml::Controls::TreeView const& control, bool on) {
    namespace xaml = winrt::Microsoft::UI::Xaml;

    if (!on) {
        control.ItemTemplate(nullptr);
        return;
    }
    control.ItemTemplate(xaml::Markup::XamlReader::Load(
                             L"<DataTemplate xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation'>"
                             L"<ContentControl Content='{Binding Content}' HorizontalContentAlignment='Left'/></DataTemplate>")
                             .as<xaml::DataTemplate>());
}

}  // namespace wxl::impl
