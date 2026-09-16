#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../generated/Microsoft.UI.Input.Enums.h"

// What stands behind `cursor = InputSystemCursorShape::SizeWestEast` on any
// element: the shape the pointer takes while it is over that element.
//
// WinUI keeps this on UIElement.ProtectedCursor, and "protected" there is a
// word for projections, not for the ABI. The metadata marks the interface
// IUIElementProtected as protected, so C# and C++/WinRT hide it from callers
// and show it only to subclasses; the object behind any element implements
// it all the same, and QueryInterface for it succeeds on a plain Border. So
// no subclass and no composition: the wrapper asks for the interface and
// sets the cursor. (C# applications reach the same setter by reflection --
// the Files explorer does, for its sidebar resizer.)
//
// Set once, for the element's lifetime: XAML shows the cursor whenever the
// pointer is over the element, so there is no Entered/Exited pair to keep.
//
// Private: the element arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_cursor(winrt::Microsoft::UI::Xaml::UIElement const& element, InputSystemCursorShape shape);

}  // namespace wxl::impl
