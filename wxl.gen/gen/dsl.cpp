#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"
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

    // small is a macro of the Windows SDK (rpcndr.h: #define small char), and the value tags of an
    // enum are spelled with the enum's own names -- CompactOverlaySize::Small is the tag
    // small. The tags are spelled below, after whatever Windows header was included first.
    std::print(file, "\n#ifdef small\n#undef small\n#endif\n");

    std::print(file, "\nnamespace wxl {{\n\nnamespace impl {{\n");

    // The values of every enumeration a property has, each written once. A tag
    // over the enumeration inherits them (impl::enum_values), the flat
    // vocabulary and the schema anchors alike, so `orientation.horizontal`
    // needs no tag type of its own. Ahead of everything below: a specialisation
    // has to precede the first Property over its enumeration.
    for (auto&& [type, values] : dsl.enumerators) {
        if (values.empty()) {
            continue;
        }
        std::print(file, "\ntemplate <>\nstruct enum_values<{}> {{\n", type);
        for (auto&& [member, enumerator] : values) {
            std::print(file, "    static constexpr {0} {1} = {0}::{2};\n", type, member, enumerator);
        }
        std::print(file, "}};\n");
    }

    // The key enums keep the metadata name; everything the DSL and the
    // wrappers spell is the member name, camelCase.
    //
    // Constrained on the call it makes, so that a class without the setter
    // refuses the assignment at the tag rather than inside this body -- and
    // so that a binding can ask, before it commits to writing, whether there
    // is anything to write to.
    for (auto&& [name, value_type] : dsl.property_value_type) {
        std::print(file, R"(
template <>
struct PropertySetter<PropertyKey::{0}> {{
    template <typename Obj, typename T>
        requires requires(Obj const& object, T const& value) {{ object.{1}(value); }}
    static void set(Obj const& object, T const& value) {{
        object.{1}(value);
    }}
}};
)",
                   name, member_name(name));
    }

    // An attached property's setter belongs to another class entirely: the
    // tag is written inside the child, and the call goes to the statics of
    // the parent whose layout the value is for. Unless the object has a
    // member of that name itself: a hand-written class may take the same
    // tag for a property of its own -- zIndex on an effect, saying where its
    // layer lies -- and then the tag is the object's.
    //
    // When several parents declare the name, the child cannot know which one
    // it will be put into, so every setter the object fits is called and the
    // others are skipped; the object has to fit at least one.
    for (auto&& [name, owners] : dsl.attached) {
        std::string calls;
        if (owners.size() == 1) {
            calls = std::format("            {}::{}(object, value);\n", owners.front().owner,
                                owners.front().setter);
        } else {
            std::string fits;
            for (auto&& attached : owners) {
                calls += std::format(
                    "            if constexpr (requires {{ {0}::{1}(object, value); }}) {{\n"
                    "                {0}::{1}(object, value);\n"
                    "            }}\n",
                    attached.owner, attached.setter);
                fits += std::format("{}requires {{ {}::{}(object, value); }}",
                                    fits.empty() ? "" : " || ", attached.owner, attached.setter);
            }
            calls += std::format(
                "            static_assert({}, \"wxl: no parent of the attached property {} takes this object\");\n",
                fits, name);
        }
        std::print(file, R"(
template <>
struct PropertySetter<PropertyKey::{0}> {{
    template <typename Obj, typename T>
    static void set(Obj const& object, T const& value) {{
        if constexpr (requires {{ object.{1}(value); }}) {{
            object.{1}(value);
        }} else {{
{2}        }}
    }}
}};
)",
                   name, member_name(name), calls);
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
        // A collection that a setter can also be given in one go --
        // `rowDefinitions[a, b]` and `rowDefinitions = L"2*,*"` are the same
        // property said two ways -- so the tag carries both.
        if (dsl.collection_element.count(name)) {
            std::print(file,
                       "inline constexpr AssignableCollectionProperty<PropertyKey::{}, {}> {};\n",
                       name, value_type.empty() ? "void" : value_type, member_name(name));
            continue;
        }

        // A class the tag builds from braces as well as takes built:
        // `titleBar = { leftHeader = ..., content = ... }`. Only with a
        // definite type -- a disagreement leaves nothing to build.
        if (dsl.braced.count(name) && !value_type.empty()) {
            std::print(file, "inline constexpr BracedProperty<PropertyKey::{}, {}> {};\n", name,
                       value_type, member_name(name));
            continue;
        }

        // A property whose declarations disagree on the type gets no
        // definite one, and with it no braced form -- `void` leaves only
        // the deduced assignment. An enum-typed property is written the same
        // way: the values it accepts come with its type (impl::enum_values), so
        // `orientation.horizontal` needs no enum name.
        std::print(file, "inline constexpr Property<PropertyKey::{}, {}> {};\n", name,
                   value_type.empty() ? "void" : value_type, member_name(name));
    }

    if (!dsl.attached.empty()) {
        std::print(file, "\n// Attached properties: written on the child, applied by the parent's\n"
                         "// statics -- `Button {{ row = 1, column = 2 }}` inside a Grid.\n");
        for (auto&& [name, owners] : dsl.attached) {
            std::print(file, "inline constexpr Property<PropertyKey::{}, {}> {};\n", name,
                       owners.front().value_type, member_name(name));
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
