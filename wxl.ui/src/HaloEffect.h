#pragma once

// wxl::HaloEffect -- a glow around the glyphs, attached to the element that
// carries it and written in that element's own braces:
//
//     TextBlock {
//         u"0",
//         fontSize = 48,
//         HaloEffect {color = rgb(92, 138, 32), blurRadius = 14.0f},
//     }
//
// A shadow whose offset is zero is a halo, and the shape it takes is whatever
// its mask is -- here the alpha of the laid-out glyphs, which getAlphaMask()
// hands over. The framework's own ThemeShadow is no use for it: no colour, no
// radius and no offset to zero, being a shadow of height rather than of light.
// So this is composition rather than XAML, and it is the one thing in a window
// that a declarative tree could not say.
//
// **No compositor is asked for.** There is none to be had where this is
// written -- the element is being constructed, and it belongs to no window
// yet. What the effect does instead is wait: XAML draws an element into a
// child of its visual, made on the first frames, and everything the halo needs
// is reachable from that visual once it exists.
//
// The tags are the DropShadow's own -- blurRadius, color, opacity, offset --
// plus zIndex, where the halo lies among the layers on the element: XAML's
// own drawing is 0, the halo's default is -1, under the glyphs; a positive
// one puts it over them. Two effects on one element are ordered by it, and
// at the same z by the order they are written.
//
// The effect is a handle: copies share their settings, and one written once
// can be attached to many elements. Each attached element gets composition
// objects of its own.

#include <cstdint>
#include <functional>

#include "core.h"
#include "Color.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "geometry.h"
#include "impl/member.h"

namespace wxl {

class HaloEffect {
public:
    /// The DropShadow's defaults: black, blur 9, opaque, no offset; under the glyphs.
    HaloEffect();

    /// The tags: color, blurRadius, opacity, offset, zIndex.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<HaloEffect, Setters...>
    explicit HaloEffect(Setters&&... setters) : HaloEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    HaloEffect(HaloEffect const& other) noexcept;
    HaloEffect& operator=(HaloEffect const& other) noexcept;
    ~HaloEffect();

    /// A bare colour is the halo's colour.
    void setPositional(Color value) const { color(value); }

    void color(Color value) const;
    void blurRadius(double value) const;
    void opacity(double value) const;
    void offset(Vector3 value) const;
    void zIndex(int32_t value) const;

    /// Attached to anything that can hand over the alpha of its own glyphs, which
    /// is what the halo is cut out of.
    template <typename Obj>
        requires requires(Obj const& element) { element.getAlphaMask(); }
    void operator()(Obj const& element) const {
        // The mask is asked for once the element is drawn: before that there
        // are no glyphs laid out to take the alpha of.
        attach(element, [element] { return element.getAlphaMask(); });
    }

private:
    void attach(UIElement const& element, std::function<CompositionBrush()> mask) const;

    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
