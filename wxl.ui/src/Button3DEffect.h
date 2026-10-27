#pragma once

// wxl::Button3DEffect -- a button drawn as a solid key under a lamp, written
// in the button's own braces:
//
//     Button {
//         u"7",
//         Button3DEffect {foreground = rgb(237, 239, 242), background = rgb(68, 72, 79),
//                         shadow = 0.7, emboss = -1},
//     }
//
// **The key.** A body of fixed shape raised `rise` over the panel: the
// facets of a rim `rim` wide along its edge, sloping outwards by `slope`
// (the tangent; `rounded` makes the facet a quarter circle), and inside them
// a face that is a dish (`emboss` below 0), flat (0) or a dome (up to 1);
// `smooth` is the width of the band where the rim's slope turns into the
// face's. The shape is written as a normal map and lit by the compositor
// through a SceneLightingEffect: `background` is the colour of the material,
// `foreground` that of the glyphs, and the black outline is drawn around
// the key, outside the light. The rounding is the button's own CornerRadius.
//
// **The light.** A distant lamp at `azimuth` degrees round the screen (0 to
// the right, 90 up) and `elevation` above it, and light from all around:
// `shadow` (0..1) is the lamp's share on a flat spot, the rest is ambient,
// so a flat spot keeps the material's colour whatever the share. `lampColor`
// and `ambientColor` are theirs; `specularAmount` and `specularShine` the
// gleam of the material. The key casts the lamp's shadow onto the panel it
// stands in -- the compositor's projected shadow, blurred by `castBlur`.
//
// **The press.** The key goes down by `travel`: its shadow on the panel
// shortens, and what has gone below the panel is hidden from the lamp by the
// edge of the opening -- a shadow lies on the top and left facets from the
// outer edge inwards, `holeShadow` of the lamp's share deep (1 takes it all),
// with a penumbra `holeSoftness` wide. Besides the movement, by the flags:
// `pressMovesLabel` moves the glyphs, the face and the inner edge of the rim
// one unit down and right while the outline stays; `pressShades` darkens the
// key by `shadeAmount`; `pressDivesLamp` lowers the spot's lamp to
// `diveDepth` of its height and brightens it `diveGain` times; `pressFlashes`
// flashes the spot. The glyphs darken by `labelShade` while pressed.
//
// **The light over the pointer.** Two lamps follow the pointer, the two a
// RevealEffect has: HoverLight shines on the faces and BorderLight on the
// rims of the keys around it, and the facets turned towards the pointer
// light up. Keys that write no light share one pair of lamps, whatever their
// kind, and so do keys given the same light: a keypad is lit as one. Written
// bare in the braces, a light takes the place of the key's own, with every
// setting it has:
//
//     HoverLight spot {size = 200, fullSize = 100, height = 30, intensity = 0.4};
//     Button3DEffect numericKey {background = rgb(68, 72, 79), spot};
//     Button3DEffect actionKey {background = rgb(48, 68, 116), spot};
//
// The effect is a handle: copies share their settings, one written once can
// be attached to a whole keypad, and a setting changed later reaches every
// key already attached.

#include "core.h"
#include "Color.h"
#include "RevealEffect.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include "impl/member.h"

namespace wxl {

class Button3DEffect
{
public:
    /// Pale glyphs on graphite, shadow 0.7, a dish; a rim of 3, raised 3 over
    /// the panel, a travel of 5; the lamp at 120 degrees, 50 high.
    Button3DEffect();

    /// The tags: foreground, background, shadow, emboss, rim, slope, rounded,
    /// smooth, rise, travel, holeShadow, holeSoftness, castBlur, azimuth,
    /// elevation, lampColor, ambientColor, specularAmount, specularShine,
    /// pressDivesLamp, pressFlashes, pressMovesLabel, pressShades, diveDepth,
    /// diveGain, shadeAmount, labelShade; bare HoverLight and BorderLight.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<Button3DEffect, Setters...>
    explicit Button3DEffect(Setters&&... setters) : Button3DEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    Button3DEffect(Button3DEffect const& other) noexcept;
    Button3DEffect& operator=(Button3DEffect const& other) noexcept;
    ~Button3DEffect();

    // The material.
    void foreground(Color value) const;
    void background(Color value) const;
    void specularAmount(double value) const;
    void specularShine(double value) const;

    // The shape.
    void emboss(double value) const;
    void rim(double value) const;
    void slope(double value) const;
    void rounded(bool value) const;
    void smooth(double value) const;
    void rise(double value) const;
    void travel(double value) const;

    // The light.
    void shadow(double value) const;
    void azimuth(double value) const;
    void elevation(double value) const;
    void lampColor(Color value) const;
    void ambientColor(Color value) const;
    void castBlur(double value) const;
    void holeShadow(double value) const;
    void holeSoftness(double value) const;

    // The press.
    void pressDivesLamp(bool value) const;
    void pressFlashes(bool value) const;
    void pressMovesLabel(bool value) const;
    void pressShades(bool value) const;
    void diveDepth(double value) const;
    void diveGain(double value) const;
    void shadeAmount(double value) const;
    void labelShade(double value) const;

    /// A bare light takes the place of the key's own. Buttons attached before
    /// it keep the light they were attached under.
    void setPositional(HoverLight const& value) const;
    void setPositional(BorderLight const& value) const;

    /// Attached to a button when its braces are applied: the body under the
    /// button's own drawing, the lights aimed at it, the shadow it casts.
    template <typename Obj>
        requires std::derived_from<Obj, Button>
    void operator()(Obj const& button) const {
        attach(button);
    }

private:
    void attach(Button const& button) const;

    struct Impl;
    core::intrusive_ptr<Impl> impl_;
};

}  // namespace wxl
