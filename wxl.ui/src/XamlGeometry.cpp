#include "impl/conversions.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

// Crossing into WinRT is where a geometry stops being text: the framework
// wants a Geometry object, and the one thing that builds it from the path
// language is the XAML converter.

namespace wxl::impl {

winrt::Microsoft::UI::Xaml::Media::Geometry to_winrt(Geometry const& value) {
    if (value.empty()) {
        return nullptr;
    }

    using winrt::Microsoft::UI::Xaml::Markup::XamlBindingHelper;
    using winrt::Microsoft::UI::Xaml::Media::Geometry;
    return XamlBindingHelper::ConvertValue(winrt::Windows::UI::Xaml::Interop::TypeName{L"Microsoft.UI.Xaml.Media.Geometry",
                                                                     winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata},
                                           winrt::box_value(to_winrt(value.data())))
        .as<Geometry>();
}

// The runtime object keeps figures, not the text that made them.
wxl::Geometry from_winrt(winrt::Microsoft::UI::Xaml::Media::Geometry const&) {
    return {};
}

}  // namespace wxl::impl
