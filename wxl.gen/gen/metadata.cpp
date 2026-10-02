#include "wxl.gen.h"

import std;

using namespace md;

namespace gen {
namespace {

TypeDef resolved(coded_index<TypeDefOrRef> const& index) {
    if (!index || index.type() == TypeDefOrRef::TypeSpec) {
        return {};  // parameterized: no interface of its own to name
    }
    return find(index);
}

}  // namespace

type_facts default_interface_of(TypeDef const& type) {
    type_facts facts;
    for (auto&& implemented : type.InterfaceImpl()) {
        // XAML classes mark their default interface with DefaultAttribute; it
        // is the interface the projection's class type stands for, so it is
        // also the one wxl's wrapper holds.
        if (!find_attribute(implemented, "DefaultAttribute")) {
            continue;
        }
        if (auto const iface = resolved(implemented.Interface())) {
            facts.default_interface = iface;
            facts.primary_field = interface_field_name(iface.TypeName());
        }
    }
    return facts;
}

}  // namespace gen
