#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

// What stands behind `wheelToParent = true` on a ScrollViewer.
//
// A ScrollViewer takes the mouse wheel for itself and marks it handled even
// when it has nothing to scroll along it: a code block in a horizontal
// ScrollViewer, which never scrolls up or down, stops the wheel dead, and the
// page around it does not move. With this on, a wheel turn the viewer cannot
// use -- no scrollable height, no modifier key held (Shift scrolls sideways,
// Ctrl zooms) -- is taken back from "handled" and goes on up the tree, to the
// panel that scrolls the page, which scrolls it as it always does. A viewer
// that does scroll vertically keeps its wheel.
//
// The handler is added once and stays: a ScrollViewer is given this when it is
// built, and false adds nothing.
//
// Private: the viewer arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_wheel_to_parent(winrt::Microsoft::UI::Xaml::Controls::ScrollViewer const& viewer, bool on);

}  // namespace wxl::impl
