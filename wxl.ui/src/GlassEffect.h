#pragma once

// wxl::GlassEffect -- a frosted pane under an element: what shows behind it,
// blurred and tinted, so that sharp glyphs and symbols stay readable over a
// picture. Written in the element's own braces:
//
//     Card {
//         GlassEffect {rgba(255, 255, 255, 0.2), blurRadius = 24.0f},
//         background = rgba(255, 255, 255, 0.35),
//         ...
//     }
//
// What is behind an element lies in two trees, and the pane is drawn in
// both: among the element's own layers, under its pixels, for whatever XAML
// draws behind it in the page; and on the window's scene, under the
// element's rectangle, for the picture a CompositionWindow shows behind the
// whole page -- the XAML island is transparent there and sees nothing of it.
// Both follow the element's rectangle and corner radius through every
// layout, come with Loaded and go with Unloaded. In a window that is not a
// CompositionWindow there is no scene, and the page pane is all there is.
//
// blurRadius is what it is on the other effects; color is the tint laid over
// the blur, a bare colour is that too; opacity is the pane as a whole. The
// effect is a handle: copies share their settings, and each attached element
// gets composition objects of its own.

#include <concepts>

#include "core.h"
#include "Color.h"
#include <wxl/Microsoft.UI.Xaml.h>
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
