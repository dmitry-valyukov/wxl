#pragma once

// wxl::BevelEffect -- a bevelled edge: light along one diagonal half of the
// border, dark along the other, with the change exactly at the two corners.
//
//     Border {
//         BorderThickness {2},
//         BevelEffect {{0x65FFFFFF, 0.0}, {0xA5000000, 1.0}},
//     }
//
// Written as a gradient by hand it very nearly works, and the way it fails is
// worth knowing. A LinearGradientBrush from {0,0} to {1,1} is relative to the
// element's box, but the lines of equal offset stay perpendicular to the axis
// **in pixels** -- so on a square they run corner to corner and on anything
// else they do not. Stretch the element and the break slides off the corners,
// and no arrangement of stops brings it back: a sharper break only makes a
// misplaced one sharper.
//
// So the axis has to be aimed, and aiming it needs the size, which is not
// known where the element is written and changes afterwards. That is the
// whole of this effect: it owns the brush, and a coroutine keeps its axis
// pointing across the corners for as long as the element lives.
//
// The arithmetic, for a box w by h. The break has to lie along the other
// diagonal, from (w,0) to (0,h), so the axis must be perpendicular to it --
// direction (h, w) in pixels rather than (w, h). Expressed back in the
// relative coordinates the brush wants, and centred so that the two remaining
// corners come out at 0 and 1:
//
//     start = (0.5 - h²/(w²+h²),  0.5 - w²/(w²+h²))
//     end   = (0.5 + h²/(w²+h²),  0.5 + w²/(w²+h²))
//
// On a square both terms are ½ and it reduces to {0,0}..{1,1}, which is what
// one would have written by hand -- the hand-written version was the special
// case all along.
//
// The stops are colour-and-offset pairs rather than built GradientStop
// objects, and that is not a matter of taste. A GradientStop is a
// DependencyObject and so belongs to exactly one collection: written once and
// worn by twenty elements, the second brush would be given a stop that is
// still in the first, and the framework refuses that outright -- "Element is
// already the child of another element". Held as data, every brush gets stops
// of its own.

#include "core.h"

#include "event_awaitable.h"
#include "generated/Members.h"
#include "generated/Microsoft.UI.Xaml.Media.h"
#include "impl/member.h"

import wxl.async;

namespace wxl {

/// One stop of a bevel, as data: the colour in ARGB and where along the
/// diagonal it sits.
struct BevelStop {
    uint32_t color = 0;
    double offset = 0;
};

class BevelEffect
{
public:
    /// Up to eight stops, in the order they run from the lit corner to the
    /// shaded one. A fixed array rather than a vector: this is written in the
    /// braces of an element, copied into a coroutine frame, and never grows.
    static constexpr std::size_t capacity = 8;

    BevelEffect(std::initializer_list<BevelStop> stops) : count_{stops.size()} {
        assert(count_ <= capacity && "wxl: a bevel takes at most eight stops");

        std::copy_n(stops.begin(), std::min(count_, capacity), stops_.begin());
    }

    /// Worn by anything with a border to paint and a size to read.
    template <typename Obj>
        requires requires(Obj const& element, Brush const& brush) {
            element.borderBrush(brush);
            element.actualWidth();
        }
    void operator()(Obj const& element) const {
        wear(element, stops_, count_);
    }

private:
    using stops_t = std::array<BevelStop, capacity>;

    // By value, as every coroutine parameter that outlives the call must be.
    template <typename Obj>
    static async::task wear(Obj element, stops_t stops, std::size_t count) {
        LinearGradientBrush const brush;
        for (std::size_t at = 0; at < count; ++at) {
            brush.gradientStops().append(
                GradientStop{ARGB{stops[at].color}, dsl::offset = stops[at].offset});
        }

        element.borderBrush(brush);

        // The loop has an end, though nothing in it looks like one. The
        // window closes while this is still waiting, the framework goes down
        // around the element, and the next thing asked of it -- its width --
        // is refused with an HRESULT. There is no element left to aim a
        // gradient at and nobody to tell, so the answer is to stop. Left to
        // escape, it would end the process: nobody holds this coroutine, so
        // there is no caller to catch anything.
        auto sizes = on_event<EventKey::SizeChanged>(element);

        while (true) {
            aim(brush, element.actualWidth(), element.actualHeight());
            if(!co_await sizes.next())
                co_return;
        }
    }

    static void aim(LinearGradientBrush const& brush, double width, double height) {
        double const diagonal = width * width + height * height;
        if (diagonal <= 0) {
            return;  // no size yet, and nothing to point at
        }

        auto const half = [diagonal](double side) { return static_cast<float>(side / diagonal); };

        float const dx = half(height * height);
        float const dy = half(width * width);

        brush.startPoint(Point{0.5f - dx, 0.5f - dy});
        brush.endPoint(Point{0.5f + dx, 0.5f + dy});
    }

    stops_t stops_{};
    std::size_t count_ = 0;
};

}  // namespace wxl
