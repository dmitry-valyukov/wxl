module;

#include <filesystem>

// A whole generator run behind one call: open the metadata the profiles
// resolved to, walk it, report what came out of the walk, and write the
// sources.
//
// It exists because main() is an ordinary translation unit. Everything the
// run works with -- the closure, the model -- is built out of winmd's types,
// and a unit that names those has to see the reader itself; keeping the run
// on this side of the boundary is what leaves main() with nothing but its
// arguments.

export module wxl.gen:run;

import :generate;
import :profile;

export {

void run(ProfileSet const& profiles, Output const& out);

}  // export
