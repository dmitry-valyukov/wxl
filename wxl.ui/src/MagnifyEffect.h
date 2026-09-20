#pragma once

// wxl::MagnifyEffect -- an element that grows under the pointer, the more the
// nearer the pointer is to its centre, written in the element's own braces:
//
//     Button {
//         u"Play",
//         MagnifyEffect {1.2f},
//     }
//
// The scale is maxScale anywhere inside an oval with the element's proportions
// and half its size, and falls off linearly from there to 1 on the ellipse
// inscribed in the element -- so there is a whole area to aim at, not a single
// point, and the element is back at its size wherever the pointer crosses its
// edge.
//
// **The motion runs in the compositor.** XAML keeps the position of the
// pointer over an element in a composition property set of its own
// (GetPointerPositionPropertySet), and an expression on the element's visual
// turns that position into a scale -- so the scale follows the pointer frame by
// frame inside the compositor, with no PointerMoved handler and no layout pass.
// Neighbours do not move, and the element keeps the place it was measured for.
// The property set keeps the last position once the pointer has left, so the
// expression is weighted by a hover value of the effect's own, animated to 1 on
// PointerEntered and back to 0 on PointerExited.
//
// **Crisp where the motion ends.** A composition scale stretches what XAML has
// drawn; a scale XAML itself applies (a ScaleTransform in RenderTransform) is
// drawn by XAML at that scale, glyph stems on device pixels. So the scale is
// split in two: a base XAML draws at, and the visual's scale divided by it. On
// PointerEntered the base becomes maxScale and the visual starts at 1/maxScale,
// so the element looks unchanged, and grows to exactly 1 -- at full size it is
// nothing but XAML's own rendering at maxScale. On PointerExited the base
// goes back to 1 and the visual starts at the scale the element had, shrinking
// to exactly 1 -- at rest it is XAML's rendering at 1. The resampling is all at
// the start of each motion, where the eye does not dwell; where it ends, nothing
// is resampled, and there is no swap of images at the end to be seen.
//
// The effect owns the element's RenderTransform and RenderTransformOrigin:
// an element that needs a render transform of its own cannot wear it. (Reading
// RenderTransform does not tell whether one was set -- an unset one comes back
// as a fresh object on every read.)

#include "generated/Microsoft.UI.Xaml.h"

namespace wxl {

class MagnifyEffect {
    /// The scale with the pointer at the very centre.
    float maxScale_ = 1.2f;

public:
    explicit MagnifyEffect(float maxScale = 1.2f)
        : maxScale_(maxScale) {}

    /// Hung on the element when its braces are applied. Out of line: a
    /// dozen calls into composition objects that every translation unit
    /// writing the effect would otherwise compile again.
    void operator()(UIElement const& element) const;
};

}  // namespace wxl
