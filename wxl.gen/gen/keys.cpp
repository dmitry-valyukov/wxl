#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

// PropertyKey.h / EventKey.h: one flat enum of every property (resp.
// event) name that survived the profile filter, under its metadata name.
// Each key stands behind one tag of the declarative syntax -- `content = ...`
// is Property<PropertyKey::Content>, `onClick = ...` Event<EventKey::Click>
// -- so a narrow profile means a correspondingly narrow enum.

namespace gen {
namespace {

// The format strings below are raw literals deliberately starting at column
// 0: everything inside a raw literal, indentation included, lands verbatim in
// the generated file, so they can't follow the surrounding code's
// indentation. Literal braces in the emitted C++ are doubled ({{ / }}) --
// that's the format string's escape, not part of the output.
void write_key_enum(std::filesystem::path const& path, std::string_view enum_name,
                    std::set<std::string> const& names) {
    auto out = open_output(path);

    std::print(out, R"({}#pragma once

namespace wxl {{

enum class {}
{{
)",
               banner, enum_name);

    for (auto&& name : names) {
        std::print(out, "    {},\n", name);
    }

    std::print(out, R"(}};

}} // namespace wxl
)");
}

}  // namespace

void write_key_enums(output const& out, model const& m, emitted& em) {
    auto const properties = out.dir / "PropertyKey.h";
    write_key_enum(properties, "PropertyKey", m.property_names);
    em.add(properties);
    std::print("generated {} ({} keys)\n", properties.string(), m.property_names.size());

    auto const events = out.dir / "EventKey.h";
    write_key_enum(events, "EventKey", m.event_names);
    em.add(events);
    std::print("generated {} ({} keys)\n", events.string(), m.event_names.size());
}

}  // namespace gen
