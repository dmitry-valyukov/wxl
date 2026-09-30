#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "../generated/Microsoft.UI.Xaml.Controls.Enums.h"

// What stands behind `leftItems` and `rightItems` having a mode.
//
// SwipeControl.LeftItems is a SwipeItems, a class that is a vector of
// SwipeItem and carries one thing besides the items -- its Mode, Reveal or
// Execute. wxl stands every such vector class for the collection itself
// (Collection<SwipeItem>), and a collection has no mode, so the mode is a
// property of the control that holds the items: it is set on the items the
// control has.
//
// Private: the control arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_left_items_mode(winrt::Microsoft::UI::Xaml::Controls::SwipeControl const& control,
                         SwipeMode mode);

void set_right_items_mode(winrt::Microsoft::UI::Xaml::Controls::SwipeControl const& control,
                          SwipeMode mode);

}  // namespace wxl::impl
