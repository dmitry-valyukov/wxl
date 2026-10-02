#pragma once

#include "generate.h"
#include "md.h"

// A whole generator run behind one call: open the metadata the profiles
// resolved to, walk it, report what came out of the walk, and write the
// sources. `types` is profiles/types.json; `symbols` the icon font's names,
// empty when the run has none.
void run(profile_set const& profiles, type_map const& types, std::vector<symbol> symbols,
         output const& out);

