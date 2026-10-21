#pragma once

#include <string_view>

#include "generate.h"
#include "md.h"

// A whole generator run behind one call: open the metadata the profiles
// resolved to, walk it, report what came out of the walk, and write the
// sources. `types` is profiles/types.json; `symbols` the icon font's names,
// empty when the run has none. `doc_language` is the language the wrappers are
// documented in, as the Windows SDK's reference names its folder (en, ru);
// empty, they are not documented.
void run(profile_set const& profiles, type_map const& types, std::vector<symbol> symbols,
         output const& out, std::string_view doc_language);

