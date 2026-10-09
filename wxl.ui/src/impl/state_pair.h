#pragma once

// The rules of the two binding pairs whose control may refuse what the field
// asks: isFocused, which an element takes only when it can, and a viewer's
// verticalOffset, which stops at the end of the content. The field always
// says what the control did -- a field left saying what was asked would make
// the next equal value silent, and the request after a refusal would never be
// heard.
//
// Templates over the element, so that the rules are checked without XAML
// (test/state_pair_test.cpp); binding.cpp hands them the real element. Each
// settle_* is the field's binding watch, and runs again from the element's
// Loaded: an element out of the tree can be asked nothing, so the field keeps
// the request and the element serves it as it enters the tree. What the
// control reports on its own -- the focus moving, a scroll coming to rest -- is
// written into the field as it is and never settled: settling a lost focus
// would take it back.

#include "../core.h"

namespace wxl::impl {

/// An element as isFocused needs it: whether it is in the tree, whether it has
/// the focus itself (not one of its children), and an attempt to take it.
template <class Element>
concept focus_target = requires(Element const& element) {
    { element.loaded() } -> std::same_as<bool>;
    { element.focused() } -> std::same_as<bool>;
    element.focus();
};

/// Makes the field and the element agree. True on an element without the focus
/// asks for it, and the field then reads whether the element took it. False on
/// a focused element is no request: XAML gives the focus up only by giving it
/// to another element, which that element's own field asks for, so the field
/// reads true again.
template <focus_target Element>
void settle_focus(Element const& element, core::observable<bool>& field) {
    if (!element.loaded() || field.get() == element.focused()) return;
    if (field.get()) element.focus();
    field.set(element.focused());
}

/// A viewer as verticalOffset needs it: whether it is in the tree, the offset
/// now, the farthest offset its content allows, and a jump that may be refused.
template <class Viewer>
concept offset_target = requires(Viewer const& viewer, double offset) {
    { viewer.loaded() } -> std::same_as<bool>;
    { viewer.offset() } -> std::same_as<double>;
    { viewer.extent() } -> std::same_as<double>;
    { viewer.scroll_to(offset) } -> std::same_as<bool>;
};

/// Makes the field and the viewer agree. A value the viewer cannot take -- past
/// the extent it knows, not a number -- is corrected in the field first (to the
/// extent, to the offset now), and the correction comes back here through the
/// watch: the viewer is asked once, for an offset it can reach, and nobody
/// hears the one it could not. A refused jump leaves the field at the offset
/// now. The offset the viewer comes to rest at arrives through ViewChanged.
template <offset_target Viewer>
void settle_offset(Viewer const& viewer, core::observable<double>& field) {
    if (!viewer.loaded()) return;
    double const now = viewer.offset();
    double const asked = field.get();
    if (asked == now) return;
    double const target =
        std::isnan(asked) ? now : std::clamp(asked, 0.0, std::max(viewer.extent(), 0.0));
    if (target != asked) {
        field.set(target);
    } else if (!viewer.scroll_to(target)) {
        field.set(now);
    }
}

}  // namespace wxl::impl
