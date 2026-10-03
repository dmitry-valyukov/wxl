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
void analyze_tags(type_map const& types, model& m) {
    m.tag_includes = {"../TaggedValue.h", "PropertyKey.h"};
    for (auto&& tag : types.tags) {
        if (m.property_names.count(tag.property_name)) {
            m.tags.push_back({tag.name, tag.value_type, tag.property_name});
            if (!tag.include.empty()) {
                m.tag_includes.insert(tag.include);
            }
        }
    }
}

void write_tags(output const& out, model const& m, emitted& em) {
    auto const path = out.dir / "Tags.h";
    auto file = open_output(path);

    std::print(file, R"({}#pragma once
)",
               banner);
    for (auto&& include : m.tag_includes) {
        std::print(file, "#include \"{}\"\n", include);
    }

    std::print(file, "\nnamespace wxl {{\n\n");
    for (auto&& tag : m.tags) {
        std::print(file, "using {} = TaggedValue<{}, PropertyKey::{}>;\n", tag.name, tag.value_type,
                   tag.property);
    }
    std::print(file, "\n}}  // namespace wxl\n");

    em.add(path);
    std::print("generated {} ({} tags)\n", path.string(), m.tags.size());
}

}  // namespace gen
