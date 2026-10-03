#pragma once

// wxl::FocusEffect -- where focus came to from the keyboard, shown in the
// window's own way rather than by the system's black-and-white frame. Attached
// to the window, like ZoomEffect, it covers every element of it, flyouts
// included:
//
//     auto const focus = FocusEffect {FocusLook::Lift};
//     CompositionWindow {
//         focus,
//         ...
//     };
//
// Two looks, and the system's own frame for comparison:
//
//   - Lift, the way a television's focus engine does it: the element rises --
//     grows a little, moves up a few pixels -- and a soft shadow opens under
//     it;
//   - Glow, what Reveal Focus was in UWP: a soft halo of the accent colour
//     around the element;
//   - System: the framework's frame, untouched.
//
// Only focus that came from the keyboard is shown, as the framework does with
// its frame: a click focuses an element as well, but the pointer already says
// where it is. An element that shows its focus itself (a list marking its
// current row) is left out with exclude().
//
// **One window, one effect, every element.** Focus is followed through the
// framework's FocusManager, which reports every element focus moves to; no
// element has to be written with anything. An element is fitted the first time
// focus comes to it from the keyboard -- its own composition objects, a layer
// under it for the shadow -- and keeps them, so the next time is a pair of
// animations. The system frame is turned off on the elements the effect
// shows, and only on them.
//
// **The motion runs in the compositor.** A level on the element's visual,
// animated from 0 to 1, drives everything shown; nothing is laid out, and only
// the focus events reach the UI thread. The scale is a factor the element's
// Scale carries -- `FocusScale` on the visual's property set -- so an element
// that also grows under the pointer (MagnifyEffect) multiplies the two rather
// than losing one.

#include "core.h"
#include "generated/Microsoft.UI.Xaml.h"

namespace wxl {

class CompositionWindow;

enum class FocusLook { System, Lift, Glow };

class FocusEffect {
public:
    /// Lift.
    FocusEffect();
    explicit FocusEffect(FocusLook look);

    FocusEffect(FocusEffect const& other) noexcept;
    FocusEffect& operator=(FocusEffect const& other) noexcept;
    ~FocusEffect();

    /// Changed later, it is the look of the next focus.
    void look(FocusLook value) const;
    void setPositional(FocusLook value) const { look(value); }

    /// Attached to the window when its braces are applied: from then on focus
    /// coming from the keyboard is shown, until the last copy of the effect goes.
    void operator()(CompositionWindow const& window) const;

    /// An element that shows its own focus: neither the effect nor the system
    /// frame is drawn on it.
    static void exclude(UIElement const& element);

private:
    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
