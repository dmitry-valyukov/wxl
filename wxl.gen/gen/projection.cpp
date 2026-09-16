module wxl.gen;

import std;

using namespace md;

namespace gen {

TypeMap::Projection const* project_type(TypeDef const& type) {
    if (!type) {
        return nullptr;
    }
    auto const name = full_name(type);
    for (auto&& projection : type_map().projections) {
        if (projection.metadata_name == name) {
            return &projection;
        }
    }
    return nullptr;
}

std::span<TypeMap::Tag const> tagged_properties() { return type_map().tags; }

}  // namespace gen
