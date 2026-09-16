#pragma once

// The lookup behind every named brush path. Hand-written for the same reason
// as its style twin next door: the generator's business is the list of
// brushes a profile's resource dictionary declares, not the code that
// resolves one. It emits `generated/brushes.h` for the paths and
// `generated/brush_names.h` for the framework's key behind each one.

#include "Microsoft.UI.Xaml.Enums.h"
#include "Microsoft.UI.Xaml.Media.h"
#include "events.h"
#include "impl/member.h"

namespace wxl::resources {

// The lookup behind every named brush, and the cache that makes it happen
// once per resource per process. Caching is sound for the same reason it is
// for styles: the application-level theme is settled at startup, so a key
// resolves to the same object for the life of the process.
Brush const& brush_at(int index);

// The same key resolved for a *named* theme, from the theme dictionaries the
// application's resources carry. This is what re-assigning after an element
// theme flip asks for: a brush assigned to a property is a value, not a
// reference the theme can retarget, so following a flip is the assigner's
// work. Dark reads the dictionary WinUI calls "Default"; Default falls back
// to the application-level lookup above.
Brush const& brush_at(int index, ElementTheme theme);

// What a brush path evaluates to. Converting it is what performs the
// lookup, so a path that is written but never used never looks anything up.
// The call form is the theme-specific lookup.
template <int Index>
struct BrushOf {
    operator Brush const&() const { return brush_at(Index); }
    Brush const& operator()(ElementTheme theme) const { return brush_at(Index, theme); }
};

// An element that has a theme of its own and says when it changes -- which is
// every FrameworkElement, and nothing else. A brush can be assigned to an
// object that is neither (a Brush inside a Brush), and there the path is read
// once and that is all there is to read.
template <typename Obj>
concept ThemeAware = requires(Obj const& object) {
    object.actualTheme();
    object.add_onActualThemeChanged(EventHandler<Object>{});
};

}  // namespace wxl::resources

namespace wxl::impl {

// A brush path is written in a description like anything else, so it joins the
// comma guard the same way -- see impl/member.h.
template <int Index>
inline constexpr bool describes<resources::BrushOf<Index>> = true;

}  // namespace wxl::impl

namespace wxl {

// A brush *path* assigned to a property follows the element's theme; a brush
// does not.
//
// The difference is what survives the assignment. `brushes.Card.…Default` is
// an index and nothing else -- BrushOf<24> has no fields -- and the index
// reaches the setter intact, because the conversion to a Brush happens only
// where a Brush is asked for. This specialization is that place, and it
// declines to convert: it resolves the index for the theme the element wears
// now, and subscribes so the same index is resolved again when that theme
// changes.
//
// That is the live link `{ThemeResource}` gives in markup. WinUI 3 has no
// runtime API that creates one -- SetResourceReference does not exist there,
// and the built-in markup extensions are not projected types -- so wxl builds
// the equivalent out of the two pieces it has: the key, kept in the type, and
// the element's own ActualThemeChanged.
//
// The handler captures nothing. `key` and `Index` are template parameters and
// the element arrives as the sender, which is the only way a wxl handler may
// reach a control at all -- a control is a temporary in the description tree,
// so a captured wrapper would keep a dead one alive.
//
// `brushes.X(theme)` is the other spelling and keeps its meaning: a brush read
// from a named theme and pinned to it, which is what OverlayCard's ink needs
// over a scrim that is dark whatever the window wears.
//
// One thing this does not yet handle: a later setter for the same property
// overriding the path with a brush of its own. The subscription outlives the
// override and puts the resource back on the next theme change, because
// nothing here can read the property to notice it no longer owns it --
// PropertySetter has set and no get. Pinning with the call form avoids it.
template <PropertyKey key, int Index, typename Owner>
struct SetterOp<key, resources::BrushOf<Index>, Owner> {
    resources::BrushOf<Index> value_;

    template <typename Obj>
    void operator()(Obj const& object) const {
        if constexpr (resources::ThemeAware<Obj>) {
            impl::PropertySetter<key>::set(object,
                                           resources::brush_at(Index, object.actualTheme()));

            // Subscribing rather than reading the theme once also settles the
            // case the manual form always got wrong: an element is built
            // outside the tree, where its ActualTheme is still the
            // application's, and learns the real one only when it is parented.
            object.add_onActualThemeChanged([](Obj const& self, Object const&) {
                impl::PropertySetter<key>::set(self,
                                               resources::brush_at(Index, self.actualTheme()));
            });
        } else {
            impl::PropertySetter<key>::set(object, resources::brush_at(Index));
        }
    }
};

}  // namespace wxl
