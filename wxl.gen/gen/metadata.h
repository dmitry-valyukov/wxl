#pragma once

#include <string>

#include "md.h"

// The default interface of a class -- what a call hands the object over as --
// and the name of the Impl field holding it. The class analysis asks once per
// class; the type mapping reads the answer from the type_index.

namespace gen {

struct type_facts {
    // The interface a call takes the object as. Empty when the class marks
    // none, or marks a parameterized one.
    md::TypeDef default_interface;

    // The Impl field holding that interface: "systemBackdrop_". Empty
    // exactly when default_interface is.
    std::string primary_field;
};

type_facts default_interface_of(md::TypeDef const& type);

}  // namespace gen
