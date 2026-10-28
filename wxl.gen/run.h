#pragma once

#include "generate.h"
#include "md.h"

struct documentation_language;

// A whole generator run behind one call: open the metadata the profiles
// resolved to, walk it, report what came out of the walk, and write the
// sources. `types` is profiles/types.json; `symbols` the icon font's names,
// empty when the run has none. `doc_language` is the language the wrappers are
// documented in, an entry of documentation_languages, and it is written with
// Doxygen's name for it to doc_language.txt in the output, for the
// documentation site; null, they are not documented and no such file is left.
void run(profile_set const& profiles, type_map const& types, std::vector<symbol> symbols,
         output const& out, documentation_language const* doc_language);

