#pragma once

#include <string>

#include "md.h"

// The default interface of a class -- what a call hands the object over as --
// and the name of the Impl field holding it. Asked by the class writer and by
// the type mapping alike, so both read one answer.

namespace gen {

struct type_facts {
    // The interface a call takes the object as. Empty when the class marks
    // none, or marks a parameterized one.
    md::TypeDef default_interface;

    // The Impl field holding that interface: "systemBackdrop_". Empty
    // exactly when default_interface is.
    std::string primary_field;
};

// Computed on first use and kept for the run -- the same type is asked about
// by several writers.
type_facts const& facts_of(md::TypeDef const& type);

}  // namespace gen
