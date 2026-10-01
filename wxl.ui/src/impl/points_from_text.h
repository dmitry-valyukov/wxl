#pragma once

#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include "../hstring_param.h"

// What stands behind `points = u"10,100 60,40"` on a Polyline or a Polygon.
//
// Points is a collection of Point, and wxl has no class for a collection of
// values; the vertices are written the way XAML writes them and the
// framework's own converter makes the PointCollection from the text.
//
// Private: the shape arrives as the projection type the wrapper already holds.

namespace wxl::impl {

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polyline const& shape, hstring_param const& text);
void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polygon const& shape, hstring_param const& text);

}  // namespace wxl::impl
