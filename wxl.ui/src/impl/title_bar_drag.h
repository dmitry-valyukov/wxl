#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../core.h"

// What stands behind `isDragRegion = true` on an element inside a TitleBar.
//
// TitleBar decides for itself which of its content drags the window and which
// takes clicks: an interactive control is clickable, anything else drags. The
// attached property overrides that for one element -- true always drags, false
// is always clickable -- and "not set" gives the choice back to the TitleBar,
// which is what an empty value means here.
//
// Private: the element arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_drag_region(winrt::Microsoft::UI::Xaml::UIElement const& element, core::nullable<bool> const& value);

}  // namespace wxl::impl
