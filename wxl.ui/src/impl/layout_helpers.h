#pragma once

// What stands behind the layout properties WinUI writes as collections of objects or as XAML's
// own mini-languages, and the links between controls that lay out together.
//
// Private: the control arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.Shapes.h>

#include "../GridLengths.h"
#include "../hstring_param.h"

namespace wxl {
class AnnotatedScrollBar;
}  // namespace wxl

namespace wxl::impl {

// A Grid's rows and columns written as RowDefinitions<u"2*,*,auto,120">, where WinUI 3 takes one
// RowDefinition object per row; wxl declares the property itself (the profile's
// syntheticMembers). The text was parsed when the code was compiled (GridLengths.h): what arrives
// is GridLength values, and building the rows has nothing left to refuse.

void set_row_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                         RowLengths const& rows);
void set_column_definitions(winrt::Microsoft::UI::Xaml::Controls::Grid const& grid,
                            ColumnLengths const& columns);

// `verticalScrollController = annotatedBar` on a ScrollView. The view scrolls through a
// ScrollPresenter, which takes a scroll controller for each direction; an AnnotatedScrollBar is
// one, through its ScrollController property, an interface no wrapper stands for. wxl writes the
// link as a property of the view, holding the bar, and makes it here.

void set_vertical_scroll_controller(winrt::Microsoft::UI::Xaml::Controls::ScrollView const& view,
                                    AnnotatedScrollBar const& bar);

void set_horizontal_scroll_controller(winrt::Microsoft::UI::Xaml::Controls::ScrollView const& view,
                                      AnnotatedScrollBar const& bar);

// `points = u"10,100 60,40"` on a Polyline or a Polygon. Points is a collection of Point, and wxl
// has no class for a collection of values; the vertices are written the way XAML writes them, and
// the framework's own converter makes the PointCollection from the text.

void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polyline const& shape,
                hstring_param const& text);
void set_points(winrt::Microsoft::UI::Xaml::Shapes::Polygon const& shape,
                hstring_param const& text);

/// `data = u"F1 M 20,20 L 24,10"` on a PathIcon or a Path. A Geometry is abstract, and the ones a
/// path is drawn with are built by the framework's own converter from XAML's mini-language and by
/// nothing simpler; the wrapper of Geometry takes a string and has this make the object.
winrt::Microsoft::UI::Xaml::Media::Geometry geometry_from_text(hstring_param const& text);

}  // namespace wxl::impl
