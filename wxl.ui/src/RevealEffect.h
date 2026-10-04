#pragma once

// wxl::RevealEffect -- light that follows the pointer: a soft spot on the
// face of the element under it and a glow along the edges of the elements
// around it. Written once and attached in the braces of every element it is
// to light:
//
//     RevealEffect reveal {
//         HoverLight {size = 140, diffuseAmount = 0.69},
//         BorderLight {size = 340, strokeThickness = 1.5},
//     };
//
//     Button {u"7", reveal}
//     Button {u"8", reveal}
//
// **Two spotlights over the pointer.** Both are the compositor's own
// SpotLight, standing `height` above the page and shining straight down on
// it. HoverLight is the narrow one and lights faces. BorderLight is the wide
// one and lights edges, so the elements next to the pointer show theirs as
// well. The lamps follow the pointer by an expression the compositor
// evaluates: while they burn, no pointer move reaches the UI thread on their
// account. They burn while the pointer is over the window's content and
// fade out when it leaves; a press on an attached element flashes the spot.
//
// **Two layers on each element.** A face layer, the element's rectangle
// with its rounding, and an edge layer, a ring `strokeThickness` wide along
// that outline, both over the element's own drawing. Neither has a colour of
// its own: what shows is the light that falls on it, its brightness turned
// into the opacity of its colour, so a layer the lamp does not reach is
// clear.
//
// **The tags of a light.** `size` is the diameter of the spot on the page,
// in layout units, and `height` how far above the page the lamp stands: the
// lamp's cone is whatever gives that spot from that height, and a lower lamp
// under the same size lights its rim at a flatter angle, so the spot fades
// sooner towards its edge. `intensity` is the lamp's, `diffuseAmount` how
// much of the light the layer gives back. `color`, `constantAttenuation` and
// `linearAttenuation` are the SpotLight's own. BorderLight also takes
// `strokeThickness`, the width of the lit edge, and CornerRadius, the
// rounding of the edge and the face; unwritten, the rounding is the
// element's own.
//
// **Handles, all three.** Copies share their settings, and a setting changed
// later reaches everything already lit: a light kept in a variable is a lamp
// that can be re-tuned while it burns. An effect written once and attached to
// a whole keypad burns one pair of lamps for it; written anew in each
// button's braces it burns a pair per button. `zIndex` is where the two
// layers lie among the layers on the element, 1 unless written: over the
// element's own drawing, which is 0.

#include <concepts>
#include <cstdint>

#include "core.h"
#include "Color.h"
#include "CornerRadius.h"
#include <wxl/Microsoft.UI.Xaml.h>
#include "impl/member.h"

namespace wxl {

class RevealEffect;

/// The narrow lamp of a RevealEffect: the spot on the face under the pointer.
class HoverLight {
public:
    /// A white spot 140 across from a lamp 120 high, intensity 1, diffuseAmount 0.69.
    HoverLight();

    /// The tags: size, height, intensity, diffuseAmount, color,
    /// constantAttenuation, linearAttenuation.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<HoverLight, Setters...>
    explicit HoverLight(Setters&&... setters) : HoverLight() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    HoverLight(HoverLight const& other) noexcept;
    HoverLight& operator=(HoverLight const& other) noexcept;
    ~HoverLight();

    void size(double value) const;
    void height(double value) const;
    void intensity(double value) const;
    void diffuseAmount(double value) const;
    void color(Color value) const;
    void constantAttenuation(double value) const;
    void linearAttenuation(double value) const;

private:
    friend class RevealEffect;

    struct State;
    core::intrusive_ptr<State> state_;
};

/// The wide lamp of a RevealEffect: the glow along the edges around the pointer.
class BorderLight {
public:
    /// A white spot 340 across from a lamp 120 high, intensity 1, diffuseAmount
    /// 1.24, an edge 1.5 wide rounded as the element is.
    BorderLight();

    /// The tags: size, height, intensity, diffuseAmount, color,
    /// constantAttenuation, linearAttenuation, strokeThickness, cornerRadius.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<BorderLight, Setters...>
    explicit BorderLight(Setters&&... setters) : BorderLight() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    BorderLight(BorderLight const& other) noexcept;
    BorderLight& operator=(BorderLight const& other) noexcept;
    ~BorderLight();

    /// A bare CornerRadius is the rounding of the edge.
    void setPositional(CornerRadius value) const { cornerRadius(value); }

    void size(double value) const;
    void height(double value) const;
    void intensity(double value) const;
    void diffuseAmount(double value) const;
    void color(Color value) const;
    void constantAttenuation(double value) const;
    void linearAttenuation(double value) const;
    void strokeThickness(double value) const;
    void cornerRadius(CornerRadius value) const;

private:
    friend class RevealEffect;

    struct State;
    core::intrusive_ptr<State> state_;
};

class RevealEffect {
public:
    /// Both lights as they come, layers over the element's own drawing.
    RevealEffect();

    /// The lights, written bare, and the tag zIndex.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<RevealEffect, Setters...>
    explicit RevealEffect(Setters&&... setters) : RevealEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    RevealEffect(RevealEffect const& other) noexcept;
    RevealEffect& operator=(RevealEffect const& other) noexcept;
    ~RevealEffect();

    /// A bare light takes the place of the effect's own. Elements attached
    /// before it keep the light they were attached under.
    void setPositional(HoverLight const& value) const;
    void setPositional(BorderLight const& value) const;

    /// Read when the effect is attached.
    void zIndex(int32_t value) const;

    template <std::derived_from<FrameworkElement> Obj>
    void operator()(Obj const& element) const {
        attach(element);
    }

private:
    void attach(FrameworkElement const& element) const;

    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
