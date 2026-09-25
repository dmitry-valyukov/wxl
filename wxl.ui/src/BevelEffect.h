#pragma once

// wxl::BevelEffect -- a bevelled rim: one colour along the top and left of
// an element, another along the bottom and right, written in the element's
// own braces:
//
//     Button {
//         u"=",
//         BevelEffect {rgba(255, 255, 255, 0.27), rgba(0, 0, 0, 0.38)},
//     }
//
// The first colour is the lit side, the second the shaded one; written the
// other way round the rim is sunk rather than raised. Tags tune it:
// strokeThickness (2), the width of the rim, laid inside the edge the way
// BorderThickness is; blurRadius (1), the width of the change from one colour
// to the other, in pixels; offset (0), how far past a corner the change sits,
// in pixels along the shorter sides -- positive lets the light run a little
// down the right side and the shade a little up the left; Margin, the rim
// moved in from the edge, or out with negative values; CornerRadius, taken
// from the element (a Border, a control, a Grid, StackPanel or RelativePanel)
// when none is written.
//
// **Drawn by the compositor, beside what XAML draws.** The rim is a
// ShapeVisual put on top among the children of the element's own visual, the
// way HaloEffect puts its shadow underneath. The element's borderBrush is left
// alone, so a button keeps its own border and gets the rim as well, and
// nothing in the tree has to be nested to carry it. The visual follows the
// element's size by itself; the rim's size, the gradient's axis and its stops
// follow by expressions the compositor evaluates, so a resize never reaches
// the UI thread.
//
// **Why the change sits on the shorter sides.** A soft change is a band
// across the gradient's axis. Put through the corners, that band runs almost
// along the long sides of a wide element and smears the light down the
// bottom and the shade up the top -- the more, the wider the element. Put a
// few pixels past the corners on the short sides, it crosses them squarely
// and stays as wide as it was written, whatever the proportions.
//
// The effect is a handle: copies share their settings, and one written once
// can be attached to a whole keypad. Each attached element gets composition
// objects of its own.

#include "core.h"
#include "Color.h"
#include "CornerRadius.h"
#include "Thickness.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "impl/member.h"

namespace wxl {

class BevelEffect {
public:
    /// White at 27 % over black at 38 %.
    BevelEffect();

    /// The colours, lit side first, then any tags.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<BevelEffect, Setters...>
    explicit BevelEffect(Setters&&... setters) : BevelEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    BevelEffect(BevelEffect const& other) noexcept;
    BevelEffect& operator=(BevelEffect const& other) noexcept;
    ~BevelEffect();

    /// A bare colour: the first is the lit side, the second the shaded one.
    void setPositional(Color value) const;
    void setPositional(CornerRadius value) const { cornerRadius(value); }

    void strokeThickness(double value) const;
    void blurRadius(double value) const;
    void offset(double value) const;
    void margin(Thickness value) const;
    void cornerRadius(CornerRadius value) const;

    /// Hung on the element when its braces are applied.
    void operator()(UIElement const& element) const;

private:
    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
