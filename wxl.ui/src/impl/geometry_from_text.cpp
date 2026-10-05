#include "geometry_from_text.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include "conversions.h"

namespace wxl::impl {

winrt::Microsoft::UI::Xaml::Media::Geometry geometry_from_text(hstring_param const& text) {
    using winrt::Microsoft::UI::Xaml::Markup::XamlBindingHelper;
    using winrt::Windows::UI::Xaml::Interop::TypeKind;
    using winrt::Windows::UI::Xaml::Interop::TypeName;

    return XamlBindingHelper::ConvertValue(TypeName{L"Microsoft.UI.Xaml.Media.Geometry", TypeKind::Metadata},
                                           winrt::box_value(to_winrt(text)))
        .as<winrt::Microsoft::UI::Xaml::Media::Geometry>();
}

}  // namespace wxl::impl
