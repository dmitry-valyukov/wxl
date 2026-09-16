#pragma once

// Thickness -- the four edges of a box, and wxl's own version of the ABI
// struct the framework declares for margins, padding and border widths.
//
// Field for field it is that struct, so a value crosses into the runtime by
// being reinterpreted rather than copied (see impl/conversions.h). What it
// adds is the three ways declarative UI wants to say four edges: one value
// for all sides, two for the horizontal and vertical pairs, four for the
// sides themselves. The two-value order is horizontal first, matching the
// two-value Thickness of XAML markup, so a value copied out of a XAML sample
// keeps its meaning.
//
// None of the constructors is explicit: `margin = {20}` is a
// copy-list-initialization of the setter's parameter, and an explicit
// constructor is not a candidate there.

#include "core.h"

namespace wxl {

struct Thickness {
    double left = 0;
    double top = 0;
    double right = 0;
    double bottom = 0;

    constexpr Thickness() = default;

    constexpr Thickness(double all) noexcept : left(all), top(all), right(all), bottom(all) {}

    constexpr Thickness(double horizontal, double vertical) noexcept
        : left(horizontal), top(vertical), right(horizontal), bottom(vertical) {}

    constexpr Thickness(double l, double t, double r, double b) noexcept
        : left(l), top(t), right(r), bottom(b) {}
};

}  // namespace wxl
