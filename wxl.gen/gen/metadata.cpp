module;

#include <format>
#include <print>

module wxl.gen;

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
        return {};  // parameterized: no class of its own to name
    }
    auto const type = find(index);
    if (!type) {
        return {};
    }
    auto const ns = type.TypeNamespace();
    if (ns == "System") {
        return {};  // System.Object / System.Enum / System.ValueType end the chain
    }
    return type;
}

type_facts compute(TypeDef const& type) {
    type_facts facts;
    facts.metadata_name = full_name(type);
    facts.name = std::string{type.TypeName()};
    facts.winrt_name = std::format("{}::{}", winrt_namespace(type.TypeNamespace()), type.TypeName());
    facts.kind = get_category(type);
    facts.base = resolved(type.Extends());

    for (auto&& implemented : type.InterfaceImpl()) {
        auto const iface = resolved(implemented.Interface());
        if (!iface) {
            continue;
        }
        facts.interfaces.push_back(iface);
        if (is_default_interface(implemented)) {
            facts.default_interface = iface;
        }
    }

    if (facts.default_interface) {
        facts.primary_field = interface_field_name(facts.default_interface.TypeName());
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
