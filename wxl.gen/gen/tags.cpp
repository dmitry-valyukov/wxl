#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

namespace gen {

// Tags.h -- the tag types the builder syntax takes as unnamed arguments for
// properties whose own type cannot identify them.
//
// One alias per tagged property the closure collected: a narrow profile that
// never reaches Padding gets no Padding tag, and nothing here names a key
// that PropertyKey.h does not declare.
void analyze_tags(Model& model) {
    model.tag_includes = {"../TaggedValue.h", "PropertyKey.h"};
    for (auto&& tag : tagged_properties()) {
        if (model.property_names.count(tag.property_name)) {
            model.tags.push_back({tag.name, tag.value_type, tag.property_name});
            if (!tag.include.empty()) {
                model.tag_includes.insert(tag.include);
            }
        }
    }
}

void write_tags(Output const& out, Model const& model, Emitted& emitted) {
    auto const path = out.dir / "Tags.h";
    auto file = open_output(path);

    std::print(file, R"({}#pragma once
)",
               banner);
    for (auto&& include : model.tag_includes) {
        std::print(file, "#include \"{}\"\n", include);
    }

    std::print(file, "\nnamespace wxl {{\n\n");
    for (auto&& tag : model.tags) {
        std::print(file, "using {} = TaggedValue<{}, PropertyKey::{}>;\n", tag.name, tag.value_type,
                   tag.property);
    }
    std::print(file, "\n}}  // namespace wxl\n");

    emitted.add(path);
    std::print("generated {} ({} tags)\n", path.string(), model.tags.size());
}

}  // namespace gen
