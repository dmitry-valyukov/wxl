#pragma once

#include <winrt/Microsoft.UI.Xaml.Media.h>

#include "../hstring_param.h"

// What stands behind `data = u"F1 M 20,20 L 24,10"` on a PathIcon or a Path.
//
// A Geometry is abstract, and the ones a path is drawn with -- PathGeometry, with
// its figures and segments -- are built by the framework's own converter from the
// mini-language of XAML and by nothing simpler. The wrapper of Geometry takes a
// string and has this function make the object from it.
//
// Private: the object is the projection's own, so this header is one only wxl's
// own sources ever include.

namespace wxl::impl {

winrt::Microsoft::UI::Xaml::Media::Geometry geometry_from_text(hstring_param const& text);

}  // namespace wxl::impl
