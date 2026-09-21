#pragma once

// wxl::MagnifyEffect -- an element that grows (or shrinks) while the pointer
// is over it, written in the element's own braces:
//
//     Button {
//         u"Play",
//         MagnifyEffect {1.2},
//     }
//
// It goes to its scale on PointerEntered and back to 1 on PointerExited, over
// a fixed time rather than with the pointer's distance from the centre. The
// distance was tried and dropped: mouse moves arrive a few per traversal, so
// the size stepped between them, and it left the element standing still at
// in-between scales, where glyphs cannot be crisp (see below).
//
// **The number and the tags.** `MagnifyEffect {1.2}` is the scale; below 1
// the element shrinks under the pointer instead. Everything else is written
// with the tags the rest of the vocabulary already has, as on any wrapper:
//
//     MagnifyEffect {
//         1.2,                   // or scale = 1.2, or scale = Size{1.3, 1.1}
//         maximum = 1.35,        // where the way in turns back
//         minimum = 0.95,        // where the way out turns back
//         duration = 250ms,      // the first phase of each motion
//         delayTime = 80ms,      // before growing: a pointer passing over
//     }                          // does not jerk the element
//
// maximum and minimum are the far points each motion swings to before
// settling: maximum past the scale on the way in, minimum past 1 on the way
// out. For a shrinking effect they lie the other way round --
// `MagnifyEffect {0.8, maximum = 0.7, minimum = 1.2}` dips to 0.7 and settles
// at 0.8, then springs to 1.2 and settles at 1. Unset, or on the wrong side,
// there is no swing.
//
// **Timing.** Without a swing a motion is one phase: 140 ms in, 180 ms out.
// With one, that phase reaches the far point and a slower settle follows,
// 200 ms in and 220 ms out. `duration` sets the first phase; the settle keeps
// its proportion to it.
//
// maximum and minimum are measured on the axis that moves further from 1. The braced `scale = {1.3, 1.1}` needs the typed
// anchor, schema::MagnifyEffect::scale in schema.h: the bare `scale` names a
// property two classes declare with two types, so on its own it only deduces.
//
// **One effect, many elements.** The object is a handle, like every wrapper:
// copies share one state, and an effect built once can be worn by a whole
// toolbar. The state keeps the composition objects every wearer can share --
// the easing curve, both motions, both expressions and the property set the
// scale lives in -- so a second button costs a property set and two
// subscriptions, not another set of animations. A setting changed later is
// picked up by every wearer at its next motion.
//
// **The motion runs in the compositor.** What the element shows is a scalar
// on the visual's own property set, animated by a key frame animation; the
// visual reads it through an expression. Nothing is measured or laid out per
// frame, and only the two pointer events reach the UI thread. Scale is the
// visual's rather than the layout's: neighbours do not move, and the element
// keeps the place it was measured for.
//
// **Crisp where the motion ends.** A composition scale stretches what XAML has
// drawn; a scale XAML itself applies (a ScaleTransform in RenderTransform) is
// drawn by XAML at that scale, glyph stems on device pixels. So the scale is
// split in two: a base XAML draws at, and the visual's scale divided by it. On
// PointerEntered the base becomes the full scale and the visual starts at its
// inverse, so the element looks unchanged, and grows to exactly 1 -- at full
// size it is nothing but XAML's own drawing. On PointerExited the base goes
// back to 1 and the visual shrinks to exactly 1 -- at rest it is XAML's
// drawing at 1. The resampling is all at the start of each motion, where the
// eye does not dwell; where it ends, nothing is resampled.
//
// The effect owns the element's RenderTransform and RenderTransformOrigin:
// an element that needs a render transform of its own cannot wear it. (Reading
// RenderTransform does not tell whether one was set -- an unset one comes back
// as a fresh object on every read.)

#include "core.h"
#include "geometry.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "impl/member.h"

namespace wxl {

class MagnifyEffect {
public:
    /// Growing to 1.2, no swing.
    MagnifyEffect();

    /// The scale alone. double, like every measurement WinUI takes -- a
    /// caller writes 1.2, not 1.2f.
    explicit MagnifyEffect(double scale);

    /// The tag form: a bare number among the tags is the scale. Numbers alone
    /// are not a tag pack -- one is the constructor above, more do not compile.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0)
                && (!(std::is_arithmetic_v<std::remove_cvref_t<Setters>> && ...))
                && impl::setter_pack<MagnifyEffect, Setters...>
    explicit MagnifyEffect(Setters&&... setters) : MagnifyEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    MagnifyEffect(MagnifyEffect const& other) noexcept;
    MagnifyEffect& operator=(MagnifyEffect const& other) noexcept;
    ~MagnifyEffect();

    /// The scale grown to, per axis.
    void scale(Size value) const;
    void scale(double value) const {
        scale(Size{static_cast<float>(value), static_cast<float>(value)});
    }
    void setPositional(double value) const { scale(value); }

    /// The far points the way in and the way out swing to, as scales on the
    /// axis that moves further. Unset, the motion does not swing.
    void maximum(double value) const;
    void minimum(double value) const;

    /// The first phase of each motion -- all of it when it does not swing.
    /// Unset, 140 ms in and 180 out; a swing adds a settle of 200 and 220.
    void duration(core::duration value) const;

    /// How long the pointer has to stay before the element starts growing.
    void delayTime(core::duration value) const;

    /// Hung on the element when its braces are applied.
    void operator()(UIElement const& element) const;

private:
    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
