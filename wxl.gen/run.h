#pragma once

#include <filesystem>

#include "generate.h"
#include "md.h"

// A whole generator run behind one call: open the metadata the profiles
// resolved to, walk it, report what came out of the walk, and write the
// sources.
void run(ProfileSet const& profiles, Output const& out);

