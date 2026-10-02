#include "wxl.gen.h"

import std;

using namespace md;

namespace gen {

type_map::projection const* project_type(TypeDef const& type, type_map const& types) {
    if (!type) {
        return nullptr;
    }
    auto const name = full_name(type);
    for (auto&& projection : types.projections) {
        if (projection.metadata_name == name) {
            return &projection;
        }
    }
    return nullptr;
}

}  // namespace gen
