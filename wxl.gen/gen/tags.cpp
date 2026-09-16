module;

#include <format>
#include <ostream>
#include <print>

module wxl.gen;

import std;

namespace gen {

// Tags.h -- the tag types the builder syntax takes as unnamed arguments for
// properties whose own type cannot identify them.
//
// One alias per tagged property the closure collected: a narrow profile that
// never reaches Padding gets no Padding tag, and nothing here names a key
// that PropertyKey.h does not declare.
void write_tags(Output const& out, Model const& model, Emitted& emitted) {
    auto const path = out.dir / "Tags.h";
    auto file = open_output(path);

    std::set<std::string> includes{"../TaggedValue.h", "PropertyKey.h"};
    std::vector<TypeMap::Tag const*> present;
    for (auto&& tag : tagged_properties()) {
        if (model.property_names.count(std::string{tag.property_name})) {
            present.push_back(&tag);
            if (!tag.include.empty()) {
                includes.insert(std::string{tag.include});
            }
        }
    }

    std::print(file, R"({}// A value that says which property it belongs to, so that the builder
// syntax can take it without the property name: `Margin{{20}}` is a
// Thickness that knows it is a margin. The property itself keeps the type
// the metadata gives it -- the tag exists only for the unnamed form.
#pragma once
)",
               banner);
    for (auto&& include : includes) {
        std::print(file, "#include \"{}\"\n", include);
    }

    std::print(file, "\nnamespace wxl {{\n\n");
    for (auto&& tag : present) {
        std::print(file, "using {} = TaggedValue<{}, PropertyKey::{}>;\n", tag->name,
                   tag->value_type, tag->property_name);
    }
    std::print(file, "\n}}  // namespace wxl\n");

    emitted.add(path);
    std::print("wrote {} ({} tags)\n", path.string(), present.size());
}

}  // namespace gen
