#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

// schema.h -- the DSL vocabulary a second time, reached through the class
// that declares it, and schema_surface.cpp, which compiles every line of it.
//
// Members.h is flat on purpose: one tag per member name, dispatch templated
// on the object, so a hundred classes cost nothing extra. What flat cannot do
// is answer "what does a Button take?", and that question is asked far more
// often than it is written down -- it is asked by typing `schema::Button::`
// and reading the list. That is the whole reason this file exists, and the
// reason it is *only* a second face on the same tags: nothing here is a new
// mechanism, every anchor still evaluates to the same op Members.h would
// have produced.
//
// Being per class buys two things the flat form cannot have. The anchor
// carries its owner, so writing one class's member on another is refused by
// name (impl::check_owner) instead of failing several expansions deeper. And
// it carries the type *that class* declares the property with -- the flat tag
// has to fall back to `void` wherever two classes disagree, and with it loses
// the braced form, which is why `startPoint = {0, 0}` never worked through
// `dsl::` and works here.
//
// AI note: everything emitted here is also exercised by the test file this
// same writer produces. A new kind of schema element (a new tag shape, a new
// member kind) has to be added in both halves below -- schema element and its
// line of test -- or the test silently stops covering it.

namespace gen {
namespace {

// Types wxl does not own, so nothing here may qualify them. Everything else
// in a value type is a wxl name.
bool is_builtin(std::string_view token) {
    static constexpr std::string_view builtins[] = {
        "bool",    "char",     "char8_t",  "char16_t", "char32_t", "wchar_t",   "short",
        "int",     "long",     "float",    "double",   "void",     "signed",    "unsigned",
        "int8_t",  "int16_t",  "int32_t",  "int64_t",  "uint8_t",  "uint16_t",  "uint32_t",
        "uint64_t", "size_t",  "ptrdiff_t", "intptr_t", "uintptr_t", "std"};

    return std::find(std::begin(builtins), std::end(builtins), token) != std::end(builtins);
}

// Every wxl name in a type spelled out from the global namespace.
//
// Inside `namespace wxl::dsl::schema` the unqualified name of nearly every
// wxl class is taken -- by the schema struct of that class. `Brush` there is
// `schema::Brush`, and a member declared with it would name the wrong type
// or, more often, fail to compile at all. Qualifying is not a defence
// against that alone: a fully spelled name is also the cheapest one for the
// compiler and for IntelliSense to resolve, which is what generated headers
// should be spending their verbosity on.
//
// A token is left alone when it is a builtin, when `std` opens the name, or
// when it is already inside one somebody else spelled out -- anything with a
// `::` before it. A token *followed* by `::` is a namespace of wxl's own
// (`core::duration`) and gets the root like any other name.
std::string qualified(std::string_view type) {
    std::string out;
    std::size_t at = 0;

    auto const identifier_char = [](char c) {
        return std::isalnum(static_cast<unsigned char>(c)) || c == '_';
    };

    while (at != type.size()) {
        if (!identifier_char(type[at])) {
            out += type[at++];
            continue;
        }

        std::size_t const start = at;
        while (at != type.size() && identifier_char(type[at])) {
            ++at;
        }

        std::string_view const token = type.substr(start, at - start);
        bool const after_colons = start >= 2 && type.substr(start - 2, 2) == "::";
        bool const numeric = std::isdigit(static_cast<unsigned char>(token.front())) != 0;

        if (numeric || after_colons || is_builtin(token)) {
            out += token;
        } else {
            out += "::wxl::";
            out += token;
        }
    }

    return out;
}

std::string property_anchor(std::string_view key, std::string_view value_type,
                            std::string_view owner) {
    return std::format("::wxl::Property<::wxl::PropertyKey::{}, {}, ::wxl::{}>", key,
                       value_type.empty() ? "void" : qualified(value_type), owner);
}

std::string collection_anchor(std::string_view key, std::string_view owner) {
    return std::format("::wxl::CollectionProperty<::wxl::PropertyKey::{}, ::wxl::{}>", key, owner);
}

std::string event_anchor(std::string_view key, std::string_view owner) {
    return std::format("::wxl::Event<::wxl::EventKey::{}, ::wxl::{}>", key, owner);
}

// One member of a class, as the schema declares it -- and, where the tag
// carries more than the anchor does, the little type that carries it.
//
// The two shapes with a body of their own are the same two Members.h has:
// an enum-typed property surfaces the values it accepts, so that
// `severity.warning` needs no enum name, and a property that is both a
// collection and assignable carries both forms at once.
struct declaration {
    std::string nested;  // the tag type, when the member needs one
    std::string anchor;  // what the static member is declared as
};

declaration declare(Schema::Class const& owner, Schema::Member const& member,
                    std::vector<Schema::Member> const& all, Dsl const& dsl) {
    using Kind = Schema::Member::Kind;

    if (member.kind == Kind::Event) {
        return {{}, event_anchor(member.key, owner.name)};
    }

    // A binding target has a plain anchor: the type it carries is the
    // observable's, and the only assignment it survives is a Bind form.
    if (member.kind == Kind::Bound) {
        return {{}, property_anchor(member.key, member.type, owner.name)};
    }

    if (member.kind == Kind::Collection) {
        // The same property said two ways -- `rowDefinitions[a, b]` and
        // `rowDefinitions = L"2*,*"`. The subscript comes from one base and
        // the assignment from the other, and the using-declaration is what
        // keeps the assignment reachable past the tag's own copy-assignment.
        auto const assignable = std::find_if(all.begin(), all.end(), [&member](auto&& other) {
            return other.kind == Kind::Property && other.key == member.key;
        });
        if (assignable == all.end()) {
            return {{}, collection_anchor(member.key, owner.name)};
        }

        std::string const property = property_anchor(member.key, assignable->type, owner.name);
        return {std::format("    struct {0}Tag : {1}, {2} {{\n        using {2}::operator=;\n"
                            "    }};\n",
                            member.key, collection_anchor(member.key, owner.name), property),
                std::format("{}Tag", member.key)};
    }

    // A property that is also a collection was written by the branch above.
    if (std::find_if(all.begin(), all.end(), [&member](auto&& other) {
            return other.kind == Kind::Collection && other.key == member.key;
        }) != all.end()) {
        return {};
    }

    if (!member.single_type) {
        return {{}, property_anchor(member.key, {}, owner.name)};
    }

    if (auto const values = dsl.enumerators.find(member.type);
        values != dsl.enumerators.end() && !values->second.empty()) {
        std::string nested =
            std::format("    struct {}Tag : {} {{\n        using Property::operator=;\n",
                        member.key, property_anchor(member.key, member.type, owner.name));
        for (auto&& [name, enumerator] : values->second) {
            nested += std::format("        static constexpr {0} {1} = {0}::{2};\n",
                                  qualified(member.type), name, enumerator);
        }
        nested += "    };\n";
        return {std::move(nested), std::format("{}Tag", member.key)};
    }

    if (member.braced) {
        return {{},
                std::format("::wxl::BracedProperty<::wxl::PropertyKey::{}, {}, ::wxl::{}>",
                            member.key, qualified(member.type), owner.name)};
    }

    return {{}, property_anchor(member.key, member.type, owner.name)};
}

void write_schema_header(std::filesystem::path const& path, Schema const& schema,
                         Dsl const& dsl) {
    auto file = open_output(path);

    std::print(file, R"({}// The same vocabulary as Members.h, reached through the class that
// declares it: `schema::Button::content` beside the bare `dsl::content`.
//
// For finding a name rather than remembering it -- `schema::Button::` offers
// exactly what a Button takes -- and for two things the flat form cannot
// carry: the anchor knows the class it was named through, so writing one
// class's member on another is refused by name, and it knows the type *that*
// class declares the property with, so the braced form survives where two
// classes disagree.
//
// Each struct mirrors its class's own base, and declares only the members
// that class declares itself; everything else arrives by inheritance, exactly
// as it does on the wrapper.
#pragma once

#include "Members.h"
)",
               banner);

    // Every class the schema covers is named by the anchors that hang on it,
    // and that is a wider set than Members.h includes: the flat vocabulary
    // names only the types properties are valued with, so a class nobody
    // assigns -- a Shape, a Rectangle -- has no header there.
    std::set<std::string> headers;
    for (auto&& klass : schema.classes) {
        if (!klass.header.empty()) {
            headers.insert(klass.header);
        }
    }
    for (auto&& header : headers) {
        std::print(file, "#include \"{}\"\n", header);
    }

    std::print(file, R"(
namespace wxl::dsl::schema {{

)");

    for (auto&& klass : schema.classes) {
        if (klass.base.empty()) {
            std::print(file, "struct {} {{\n", klass.name);
        } else {
            std::print(file, "struct {} : ::wxl::dsl::schema::{} {{\n", klass.name, klass.base);
        }

        for (auto&& member : klass.members) {
            auto const [nested, anchor] = declare(klass, member, klass.members, dsl);
            if (anchor.empty()) {
                continue;
            }

            if (!nested.empty()) {
                std::print(file, "{}", nested);
            }
            std::print(file, "    static constexpr {} {}{{}};\n", anchor, member.name);
        }

        std::print(file, "}};\n\n");
    }

    std::print(file, "}}  // namespace wxl::dsl::schema\n");
}

// The value a test line hands the member. A property takes one of its own
// type; an enum-typed one takes it through the tag, which proves the
// enumerators the tag carries are reachable as well.
std::string test_value(Schema::Class const& owner, Schema::Member const& member, Dsl const& dsl) {
    if (auto const values = dsl.enumerators.find(member.type);
        member.single_type && values != dsl.enumerators.end() && !values->second.empty()) {
        return std::format("::wxl::dsl::schema::{}::{}.{}", owner.name, member.name,
                           values->second.front().first);
    }

    return "value";
}

void write_schema_test(std::filesystem::path const& path, Schema const& schema, Dsl const& dsl,
                       std::size_t& lines) {
    auto file = open_output(path);

    std::print(file, R"({}// One line per element of schema.h, and nothing else.
//
// Compile-only, like the surface test beside it: none of these functions is
// called, and the WinUI runtime is not up in a test anyway. What it proves is
// that every anchor the schema offers actually applies to the class it hangs
// on -- that the key reaches a setter that class has, that the value type the
// schema names is one that setter takes, and that the owner check passes for
// the class itself.
//
// It is generated from the same closure as the schema, which is what makes
// the coverage rule enforceable: a new type or a new member appears in both
// files or in neither.
//
// The one thing not written here is the negative: that the owner check
// *refuses* another class's member. It cannot be -- the check is a
// static_assert inside a function body, and a body is not instantiated by a
// requires-expression, so there is no way to assert that it fires.

#include "../Bind.h"
#include "schema.h"

namespace {{

)",
               banner);

    for (auto&& klass : schema.classes) {
        if (klass.members.empty()) {
            continue;
        }

        std::print(file, "// {}\n", klass.name);

        for (auto&& member : klass.members) {
            std::string const object = std::format("::wxl::{} const& object", klass.name);
            std::string const path_to = std::format("::wxl::dsl::schema::{}::{}", klass.name,
                                                    member.name);

            switch (member.kind) {
                case Schema::Member::Kind::Event:
                    std::print(file, R"([[maybe_unused]] void {0}_{1}({2}) {{
    ::wxl::impl::apply_argument(object, {3} = [](auto const&, auto&) {{}});
}}
)",
                               klass.name, member.name, object, path_to);
                    break;

                case Schema::Member::Kind::Collection:
                    std::print(file, R"([[maybe_unused]] void {0}_{1}({2}, {4} const& item) {{
    ::wxl::impl::apply_argument(object, {3}[item]);
}}
)",
                               klass.name, member.name, object, path_to, qualified(member.type));
                    break;

                case Schema::Member::Kind::Property:
                    std::print(file, R"([[maybe_unused]] void {0}_{1}_assigned({2}, {4} value) {{
    ::wxl::impl::apply_argument(object, {3} = {5});
}}
)",
                               klass.name, member.name, object, path_to,
                               member.type.empty() ? "::wxl::Object" : qualified(member.type),
                               test_value(klass, member, dsl));
                    break;

                // Bound in the direction its pair takes: the one line that
                // proves the anchor, the pair and the observable's type agree.
                case Schema::Member::Kind::Bound:
                    std::print(file, R"([[maybe_unused]] void {0}_{1}_bound({2}, ::wxl::core::observable<{4}>& field) {{
    ::wxl::impl::apply_argument(object, {3} = ::wxl::{5}{{field}});
}}
)",
                               klass.name, member.name, object, path_to, qualified(member.type),
                               member.direction == "input"    ? "BindInput"
                               : member.direction == "output" ? "BindOutput"
                                                              : "Bind");
                    break;
            }

            ++lines;
        }

        std::print(file, "\n");
    }

    std::print(file, "}}  // namespace\n");
}

}  // namespace

void write_schema(Output const& out, Schema const& schema, Dsl const& dsl, Emitted& emitted) {
    auto const header = out.dir / "schema.h";
    write_schema_header(header, schema, dsl);
    emitted.add(header);

    std::size_t members = 0;
    for (auto&& klass : schema.classes) {
        members += klass.members.size();
    }

    auto const test = out.dir / "schema_surface.cpp";
    std::size_t lines = 0;
    write_schema_test(test, schema, dsl, lines);
    emitted.add(test, std::format("{}.surface-test", out.cmake_target));

    std::print("wrote {} ({} classes, {} members)\n", header.string(), schema.classes.size(),
               members);
    std::print("wrote {} ({} checks)\n", test.string(), lines);
}

}  // namespace gen
