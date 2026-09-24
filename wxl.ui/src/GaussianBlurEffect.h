#pragma once

// wxl::GaussianBlurEffect -- a glow around the glyphs with a falloff of the
// application's choosing, written in the element's own braces:
//
//     TextBlock {
//         u"7",
//         GaussianBlurEffect {color = rgb(0, 0, 0), blurRadius = 3.0f, gamma = 0.5},
//     }
//
// The halo a DropShadow gives (HaloEffect) fades the way a Gaussian fades,
// and its density tops out at opacity 1: a dense, tight glow needed two of
// them. This one is the same glow drawn by an effect graph -- the alpha of
// the glyphs blurred, then bent by `gamma`, then filled with the colour --
// so the falloff is a knob. `gamma` is the exponent on the blurred alpha: 1
// is the Gaussian as it is, below 1 lifts the middle of the fade and makes
// the glow dense right up to its edge, above 1 thins it to a faint corona.
//
// blurRadius, color and opacity mean what they mean on HaloEffect; zIndex
// says where the glow lies among the layers on the element, XAML's own
// drawing being 0 and the default -1, under the glyphs. Worn by anything
// that hands over the alpha of its glyphs. The effect is a handle: copies
// share their settings, and each wearer gets composition objects of its own.

#include <cstdint>
#include <functional>

#include "core.h"
#include "Color.h"
#include "generated/Microsoft.UI.Composition.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "impl/member.h"

namespace wxl {

class GaussianBlurEffect {
public:
    /// Black, blur 9, opaque, gamma 1; under the glyphs.
    GaussianBlurEffect();

    /// The tags: color, blurRadius, opacity, gamma, zIndex.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<GaussianBlurEffect, Setters...>
    explicit GaussianBlurEffect(Setters&&... setters) : GaussianBlurEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    GaussianBlurEffect(GaussianBlurEffect const& other) noexcept;
    GaussianBlurEffect& operator=(GaussianBlurEffect const& other) noexcept;
    ~GaussianBlurEffect();

    /// A bare colour is the glow's colour.
    void setPositional(Color value) const { color(value); }

    void color(Color value) const;
    void blurRadius(double value) const;
    void opacity(double value) const;
    void gamma(double value) const;
    void zIndex(int32_t value) const;

    template <typename Obj>
        requires requires(Obj const& element) { element.getAlphaMask(); }
    void operator()(Obj const& element) const {
        // The mask is asked for once the element is drawn: before that there
        // are no glyphs laid out to take the alpha of.
        wear(element, [element] { return element.getAlphaMask(); });
    }

private:
    void wear(UIElement const& element, std::function<CompositionBrush()> mask) const;

    struct State;
    core::intrusive_ptr<State> state_;
};

}  // namespace wxl
