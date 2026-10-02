#pragma once

#include "md.h"

// The question the type map answers for every analysis that names a type:
// does wxl already own an equivalent of this WinRT type.
//
// The table is data, read from profiles/types.json (see type_map in
// profile.h) -- this is only where the analysis asks about it. Everything
// that names a type must consult project_type() *before* falling back to
// the registry of generated names, or the projection would only half apply.

namespace gen {

// The projection for `type`, or nullptr if wxl generates a wrapper for it
// like any other type.
type_map::projection const* project_type(md::TypeDef const& type, type_map const& types);

}  // namespace gen
