#pragma once

// wxl::Button3DEffect -- a button drawn as a solid thing under a lamp,
// written in the button's own braces:
//
//     Button {
//         u"7",
//         Button3DEffect {foreground = rgb(237, 239, 242), background = rgb(68, 72, 79),
//                         shadow = 0.7, emboss = -1},
//     }
//
// `background` is the colour of the button's material and `foreground` that of
// its glyphs; everything the eye sees is derived from those two by the light:
// the face in its three states (at rest, under the pointer, pressed), the
// dark outline, the thin rim of light and shade along the edges. `shadow`
// (0..1, 0.7) is how much of the light is the lamp's and how much comes from
// all around: at 0 the button is lit evenly and looks flat, at 1 its shadows
// are black. `emboss` (-1..1, -1) is the shape of the face: negative a dish
// sunk into the panel, its shadow gathered at the edge under the lamp;
// positive a dome standing out of it; 0 a flat face.
//
// The lamp stands above the button and a little to the left, at 120 degrees
// round the screen and 50 above it, warm, with a cool ambient -- the same
// lamp for every button the effect is attached to, so a keypad is lit as one.
//
// The arithmetic is relief_helper's, run when the effect is attached: a button's
// tags are values of the running program, not constants. What is applied is
// the same set of setters the button could have been given by hand -- a
// preset in all but name -- and the rim is a BevelEffect attached to the button
// with it. The effect is a handle: copies share their settings, and one
// written once can be attached to a whole keypad.
//
// **The light over the pointer.** A second pair of lamps follows the pointer,
// the two a RevealEffect has: HoverLight puts a spot on the face of the key
// under it, BorderLight a glow along the edges of the keys around. A key
// takes them as it comes, and takes them dimmer than a plain button does --
// diffuseAmount 0.5 for the spot and 0.9 for the edges, where RevealEffect's
// own are 0.69 and 1.24: a coloured key under its fixed lamp is bright
// already. Written bare in the braces, a light takes the place of the key's
// own, with every setting it has:
//
//     HoverLight spot {size = 200, diffuseAmount = 0.4};
//     Button3DEffect numericKey {background = rgb(68, 72, 79), spot};
//     Button3DEffect actionKey {background = rgb(48, 68, 116), spot};
//
// Keys that write no light share one pair of lamps, whatever their kind, and
// so do keys given the same light: a keypad is lit as one.

#include "core.h"
#include "Color.h"
#include "RevealEffect.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "impl/member.h"

namespace wxl {

class Button3DEffect {
public:
    /// Pale glyphs on graphite, shadow 0.7, a dish.
    Button3DEffect();

    /// The tags: foreground, background, shadow, emboss; bare HoverLight and BorderLight.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<Button3DEffect, Setters...>
    explicit Button3DEffect(Setters&&... setters) : Button3DEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    Button3DEffect(Button3DEffect const& other) noexcept;
    Button3DEffect& operator=(Button3DEffect const& other) noexcept;
    ~Button3DEffect();

    void foreground(Color value) const;
    void background(Color value) const;
    void shadow(double value) const;
    void emboss(double value) const;

    /// A bare light takes the place of the key's own. Buttons attached before
    /// it keep the light they were attached under.
    void setPositional(HoverLight const& value) const;
    void setPositional(BorderLight const& value) const;

    /// Attached to a button when its braces are applied: the face, the outline,
    /// the state brushes, the rim, and the pointer's light.
    template <typename Obj>
        requires std::derived_from<Obj, Button>
    void operator()(Obj const& button) const {
        attach(button);
    }

private:
    void attach(Button const& button) const;

    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
