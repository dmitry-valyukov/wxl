module;
#include <format>
#include <ostream>
#include <print>

module wxl.gen;
import std;
// Members.h -- the surface the builder syntax is written against.
//
// Two things per member, and nothing per class. A tag object (`Content`,
// `OnClick`) is what the DSL writes on the left of the `=`; behind it, one
// PropertySetter / EventAdder specialisation says which member of the
// object the assignment reaches. Both halves of the dispatch are templates
// on the object type, so a single specialisation serves every class that
// declares that member -- which is the whole reason the key enums are flat
// rather than per class.
//
// AI note: this file emits *only* names that came out of the profile-driven
// closure, so a narrow profile yields a correspondingly narrow DSL surface.
// The tags are `inline constexpr` empty objects: they cost nothing at
// runtime and exist purely to give the assignment a left-hand side.
namespace gen {
void write_dsl(Output const& out, Dsl const& dsl, Emitted& emitted) {
    auto const path = out.dir / "Members.h";
    auto file = open_output(path);
    std::print(file, R"({}// The builder syntax's vocabulary: one tag per property and per event,
// and the dispatch that turns an assignment to a tag into a call on the
// object the enclosing constructor is building.
#pragma once

#include "../impl/member.h"
#include "Tags.h"
)",
               banner);
    for (auto&& include : dsl.includes) {
        if (include.starts_with('<')) {
            std::print(file, "#include {}\n", include);
        } else {
            std::print(file, "#include \"{}\"\n", include);
        }
    }

    std::print(file, "\nnamespace wxl {{\n\nnamespace impl {{\n");

    // The key enums keep the metadata name; everything the DSL and the
    // wrappers spell is the member name, camelCase.
    for (auto&& [name, value_type] : dsl.property_value_type) {
        std::print(file, R"(
template <>
struct PropertySetter<PropertyKey::{0}> {{
    template <typename Obj, typename T>
    static void set(Obj const& object, T const& value) {{
        object.{1}(value);
    }}
}};
)",
                   name, member_name(name));
    }

    // An attached property's setter belongs to another class entirely: the
    // tag is written inside the child, and the call goes to the statics of
    // the parent whose layout the value is for.
    for (auto&& [name, attached] : dsl.attached) {
        std::print(file, R"(
template <>
struct PropertySetter<PropertyKey::{0}> {{
    template <typename Obj, typename T>
    static void set(Obj const& object, T const& value) {{
        {1}::{2}(object, value);
    }}
}};
)",
                   name, attached.owner, attached.setter);
    }

    // A collection-valued property is filled, not assigned: the setter is
    // handed the whole run of items, so it can reach the collection once and
    // decide for itself how to put them in.
    for (auto&& [name, element] : dsl.collection_element) {
        std::print(file, R"(
template <>
struct CollectionSetter<PropertyKey::{0}> {{
    template <typename Obj, typename... Items>
    static void set(Obj const& object, Items const&... items) {{
        auto const collection = object.{1}();
        (collection.append(items), ...);
    }}
}};
)",
                   name, member_name(name));
    }

    for (auto&& name : dsl.events) {
        std::print(file, R"(
template <>
struct EventAdder<EventKey::{0}> {{
    template <typename Obj, typename Fn>
    static EventToken add(Obj const& object, Fn const& handler) {{
        return object.add_on{0}(handler);
    }}

    template <typename Obj>
    static void remove(Obj const& object, EventToken token) {{
        object.remove_on{0}(token);
    }}

    template <typename Obj>
    using args_t = event_args_of_t<decltype(&Obj::add_on{0})>;
}};
)",
                   name);
    }

    // The tags live in a namespace of their own so that consuming code brings
    // the vocabulary in deliberately rather than having every property name in
    // scope the moment it names a wxl type.
    std::print(file, "\n}}  // namespace impl\n\nnamespace dsl {{\n\n// Property tags.\n");
    for (auto&& [name, value_type] : dsl.property_value_type) {
        if (dsl.collection_element.count(name)) {
            // A collection that a setter can also be given in one go --
            // `rowDefinitions[a, b]` and `rowDefinitions = L"2*,*"` are the
            // same property said two ways -- so the tag carries both. The
            // subscript comes from one base, the assignment from the other,
            // and the using-declaration is what keeps the assignment
            // reachable past the tag's own copy-assignment.
            std::print(file, R"(
struct {0}Tag : CollectionProperty<PropertyKey::{0}>, Property<PropertyKey::{0}, {1}> {{
    using Property<PropertyKey::{0}, {1}>::operator=;
}};
inline constexpr {0}Tag {2};
)",
                       name, value_type.empty() ? "void" : value_type, member_name(name));
            continue;
        }
        // An enum-typed tag carries the values it accepts, so the enum type
        // is named once here instead of at every use:
        // `orientation.horizontal`, not `orientation = Orientation::Horizontal`.
        // The using-declaration is what keeps the assignments reachable --
        // the derived tag's own copy-assignment would hide them.
        if (auto const values = dsl.enumerators.find(value_type);
            values != dsl.enumerators.end() && !values->second.empty()) {
            std::print(file, R"(
struct {0}Tag : Property<PropertyKey::{0}, {1}> {{
    using Property::operator=;
)",
                       name, value_type);
            for (auto&& [member, enumerator] : values->second) {
                std::print(file, "    static constexpr {0} {1} = {0}::{2};\n", value_type, member,
                           enumerator);
            }
            std::print(file, "}};\ninline constexpr {}Tag {};\n\n", name, member_name(name));
            continue;
        }

        // A property whose declarations disagree on the type gets no
        // definite one, and with it no braced form -- `void` leaves only
        // the deduced assignment.
        std::print(file, "inline constexpr Property<PropertyKey::{}, {}> {};\n", name,
                   value_type.empty() ? "void" : value_type, member_name(name));
    }

    if (!dsl.attached.empty()) {
        std::print(file, "\n// Attached properties: written on the child, applied by the parent's\n"
                         "// statics -- `Button {{ row = 1, column = 2 }}` inside a Grid.\n");
        for (auto&& [name, attached] : dsl.attached) {
            std::print(file, "inline constexpr Property<PropertyKey::{}, {}> {};\n", name,
                       attached.value_type, member_name(name));
        }
    }

    if (!dsl.collection_element.empty()) {
        std::print(file, "\n// Collection tags, subscripted rather than assigned to:\n"
                         "// `children[first, second]`.\n");
        for (auto&& [name, element] : dsl.collection_element) {
            if (dsl.property_value_type.count(name)) {
                continue;  // already written above, carrying both forms
            }
            std::print(file, "inline constexpr CollectionProperty<PropertyKey::{}> {};\n", name,
                       member_name(name));
        }
    }

    std::print(file, "\n// Event tags: `on` + the metadata name, so an event is visibly not a\n"
                     "// property inside the same braces.\n");
    for (auto&& name : dsl.events) {
        std::print(file, "inline constexpr Event<EventKey::{}> on{};\n", name, name);
    }

    std::print(file, "\n}}  // namespace dsl\n}}  // namespace wxl\n");

    emitted.add(path);
    std::print("wrote {} ({} property tags, {} collection tags, {} event tags)\n", path.string(),
               dsl.property_value_type.size(), dsl.collection_element.size(), dsl.events.size());
}

}  // namespace gen
