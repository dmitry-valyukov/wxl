#pragma once

// CornerRadius -- the four corners of a box, and wxl's own version of the
// ABI struct the framework declares for corner rounding.
//
// Field for field it is that struct, so a value crosses into the runtime by
// being reinterpreted rather than copied (see impl/conversions.h). What it
// adds is the two ways declarative UI says corners: one value for all four
// -- the everyday case, CornerRadius{12} -- and four for the corners
// themselves, clockwise from top-left, the order the framework declares.
//
// The single-value constructor is explicit, unlike Thickness's: a Thickness
// is routed by a tag (Margin{20}), while a CornerRadius routes by its own
// type, and a lone double that quietly became a radius would read as
// anything. The price is that `cornerRadius = {12}` is not a candidate --
// the positional CornerRadius{12} is the intended spelling.

#include "core.h"

namespace wxl {

struct CornerRadius {
    double topLeft = 0;
    double topRight = 0;
    double bottomRight = 0;
    double bottomLeft = 0;

    constexpr explicit CornerRadius(double all = 0) noexcept
        : topLeft(all), topRight(all), bottomRight(all), bottomLeft(all) {}

    constexpr CornerRadius(double topLeft, double topRight, double bottomRight,
                           double bottomLeft) noexcept
        : topLeft(topLeft), topRight(topRight), bottomRight(bottomRight), bottomLeft(bottomLeft) {}
};

}  // namespace wxl
