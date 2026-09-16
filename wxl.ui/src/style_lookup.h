#pragma once

// The lookup behind every named style path. Hand-written, and next to the
// generator's output rather than inside it: not a line here depends on which
// styles a profile's resource dictionary happens to declare, so the
// generator emits only what does -- `generated/styles.h` for the paths and
// `generated/style_names.h` for the framework's key behind each one.

#include "Microsoft.UI.Xaml.h"
#include "impl/member.h"

namespace wxl::resources {

// The lookup behind every named style, and the cache that makes it happen
// once per resource per process. One function for all of them: a resource is
// nothing but its position in a table, so naming one costs no code at all.
Style const& style_at(int index);

// What a resource path evaluates to. Converting it is what performs the
// lookup, the way subscripting a map is what inserts -- so a path that is
// written but never used as a Style never looks anything up.
template <int Index>
struct StyleOf {
    operator Style const&() const { return style_at(Index); }
};

}  // namespace wxl::resources

namespace wxl::impl {

// A style path is written in a description like anything else, so it joins the
// comma guard the same way -- see impl/member.h.
template <int Index>
inline constexpr bool describes<resources::StyleOf<Index>> = true;

}  // namespace wxl::impl
