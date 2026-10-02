#include "wxl.gen.h"

import std;

using namespace md;

namespace gen {
namespace {

// XAML classes mark their default interface with DefaultAttribute; it is the
// interface the projection's class type stands for, so it is also the one
// wxl's wrapper holds.
bool is_default_interface(InterfaceImpl const& implemented) {
    for (auto&& attribute : implemented.CustomAttribute()) {
        auto const [ns, name] = attribute.TypeNamespaceAndName();
        if (ns == "Windows.Foundation.Metadata" && name == "DefaultAttribute") {
            return true;
        }
    }
    return false;
}

TypeDef resolved(coded_index<TypeDefOrRef> const& index) {
    if (!index || index.type() == TypeDefOrRef::TypeSpec) {
        return {};  // parameterized: no interface of its own to name
    }
    return find(index);
}

type_facts compute(TypeDef const& type) {
    type_facts facts;
    for (auto&& implemented : type.InterfaceImpl()) {
        if (!is_default_interface(implemented)) {
            continue;
        }
        if (auto const iface = resolved(implemented.Interface())) {
            facts.default_interface = iface;
            facts.primary_field = interface_field_name(iface.TypeName());
        }
    }
    return facts;
}

}  // namespace

type_facts const& facts_of(TypeDef const& type) {
    static std::map<TypeDef, type_facts> known;

    auto const [entry, added] = known.try_emplace(type);
    if (added) {
        entry->second = compute(type);
    }
    return entry->second;
}

}  // namespace gen
