#include "swipe_items_mode.h"

namespace wxl::impl {

namespace controls = winrt::Microsoft::UI::Xaml::Controls;

// A SwipeControl has no items until someone gives it some: the collection is
// made here, empty, for the items to be added to afterwards.
void set_left_items_mode(controls::SwipeControl const& control, SwipeMode mode) {
    if (!control.LeftItems()) {
        control.LeftItems(controls::SwipeItems{});
    }
    control.LeftItems().Mode(static_cast<controls::SwipeMode>(mode));
}

void set_right_items_mode(controls::SwipeControl const& control, SwipeMode mode) {
    if (!control.RightItems()) {
        control.RightItems(controls::SwipeItems{});
    }
    control.RightItems().Mode(static_cast<controls::SwipeMode>(mode));
}

}  // namespace wxl::impl
