module;

#include <span>

// The two questions the type map answers for the writers: does wxl already
// own an equivalent of this WinRT type, and does this property carry a tag
// of its own.
//
// Both tables are data, read from profiles/types.json (see TypeMap in
// profile.h) -- this is only where the writers ask about them. Everything
// that names a type must consult project_type() *before* falling back to
// the registry of generated names, or the projection would only half apply.

export module wxl.gen:projection;

import :profile;
import :md;
export namespace gen {

// The projection for `type`, or nullptr if wxl generates a wrapper for it
// like any other type.
TypeMap::Projection const* project_type(md::TypeDef const& type);

// Every tagged property. The tags writer emits an alias for each one whose
// property the closure actually collected.
std::span<TypeMap::Tag const> tagged_properties();

}  // namespace gen
