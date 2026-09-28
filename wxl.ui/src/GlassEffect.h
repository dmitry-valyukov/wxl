#pragma once

// wxl::GlassEffect -- a frosted pane under an element: what the window shows
// behind it, blurred and tinted, so that sharp glyphs and symbols stay
// readable over a picture. Written in the element's own braces:
//
//     Card {
//         GlassEffect {rgba(255, 255, 255, 0.2), blurRadius = 24.0f},
//         background = rgba(255, 255, 255, 0.35),
//         ...
//     }
//
// The pane is drawn on the window's scene, where the picture is, under the
// XAML island: an element's own visual sees nothing behind it, since the
// island is transparent, and the scene sees everything the window paints.
// The pane follows the element's rectangle and corner radius through every
// layout, comes with Loaded and goes with Unloaded. In a window that is not a
// CompositionWindow there is no scene, and the effect does nothing.
//
// blurRadius is what it is on the other effects; color is the tint laid over
// the blur, a bare colour is that too; opacity is the pane as a whole. The
// effect is a handle: copies share their settings, and each attached element
// gets composition objects of its own.

#include <concepts>

#include "core.h"
#include "Color.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "impl/member.h"

namespace wxl {

class GlassEffect {
public:
    /// No tint, blur 24, opaque pane.
    GlassEffect();

    /// The tags: color, blurRadius, opacity.
    template <typename... Setters>
        requires(sizeof...(Setters) > 0) && impl::setter_pack<GlassEffect, Setters...>
    explicit GlassEffect(Setters&&... setters) : GlassEffect() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    GlassEffect(GlassEffect const& other) noexcept;
    GlassEffect& operator=(GlassEffect const& other) noexcept;
    ~GlassEffect();

    /// A bare colour is the tint.
    void setPositional(Color value) const { color(value); }

    void color(Color value) const;
    void blurRadius(double value) const;
    void opacity(double value) const;

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
