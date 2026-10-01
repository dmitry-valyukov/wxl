#include "points_from_text.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Windows.UI.Xaml.Interop.h>

#include "conversions.h"

namespace wxl::impl {

namespace {

winrt::Microsoft::UI::Xaml::Media::PointCollection points_from_text(hstring_param const& text) {
    using winrt::Windows::UI::Xaml::Interop::TypeKind;
    using winrt::Windows::UI::Xaml::Interop::TypeName;

    return winrt::Microsoft::UI::Xaml::Markup::XamlBindingHelper::ConvertValue(
               TypeName{L"Microsoft.UI.Xaml.Media.PointCollection", TypeKind::Metadata},
               winrt::box_value(to_winrt(text)))
        .as<winrt::Microsoft::UI::Xaml::Media::PointCollection>();
}

}  // namespace

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polyline const& shape, hstring_param const& text) {
    shape.Points(points_from_text(text));
}

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polygon const& shape, hstring_param const& text) {
    shape.Points(points_from_text(text));
}

}  // namespace wxl::impl
