#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

// Enums and structs, emitted flat into `namespace wxl`, grouped into one
// file pair per *source* WinRT namespace (Microsoft.UI.Xaml.Enums.h /
// Microsoft.UI.Xaml.Structs.h, ...), plus umbrella files (Enums.h /
// Structs.h) that #include every per-namespace file in turn, and
// Structs.impl.h with the conversions of every struct. File and type names
// follow WinUI/WinRT's own PascalCase convention throughout.
//
// Types wxl already has an equivalent of (Size/Point/Rect -> wxl::geometry,
// see gen/projection.h) are not emitted: they are skipped as definitions and
// referred to by their projected name wherever a field mentions them.

using namespace md;

namespace gen {
namespace {

// Every alternative Constant::Value() can hold for an enum literal is
// arithmetic, and the value is what the generated enumerator is written as.
// The widening is what makes one code path do: std::format has no formatter
// for char16_t (WinRT's Char), and would print a char as a character rather
// than as the number an enumerator needs.
std::string format_constant(Constant const& constant) {
    return std::visit(
        [](auto&& value) -> std::string {
            using T = std::decay_t<decltype(value)>;
            if constexpr (std::is_floating_point_v<T>) {
                return std::format("{}", value);
            } else if constexpr (std::is_signed_v<T>) {
                return std::format("{}", static_cast<long long>(value));
            } else if constexpr (std::is_arithmetic_v<T>) {
                return std::format("{}", static_cast<unsigned long long>(value));
            } else {
                return "0";  // not expected for an enum literal
            }
        },
        constant.Value());
}

// The enumerators the profiles kept, and the type underneath. `kept` is what
// the walk recorded as the enum's members: the enumerators the profiles kept
// -- all of them for an enum they never name.
struct enum_literals {
    char const* underlying = "int32_t";
    std::vector<Field> literals;
};

enum_literals literals_of(TypeDef const& type, std::map<TypeDef, std::set<std::string>> const& kept) {
    enum_literals found;
    auto const values = kept.find(type);
    for (auto&& field : type.FieldList()) {
        if (field.Flags().Literal()) {
            if (values != kept.end() && values->second.contains(std::string{field.Name()})) {
                found.literals.push_back(field);
            }
        } else if (field.Signature().Type().element_type() == ElementType::U4) {
            // The non-literal "value__" backing field's own type is the
            // enum's real underlying type -- WinRT enums are Int32
            // unless [FlagsAttribute] makes them UInt32.
            found.underlying = "uint32_t";
        }
    }
    return found;
}

enum_info analyze_enum(TypeDef const& type, std::map<TypeDef, std::set<std::string>> const& kept) {
    auto const found = literals_of(type, kept);
    enum_info info{std::string{type.TypeName()}, found.underlying, {}};
    for (auto&& literal : found.literals) {
        info.values.push_back({std::string{literal.Name()}, format_constant(literal.Constant())});
    }
    return info;
}

// A resolved struct field: a primitive C++ type, a projected wxl type
// (geometry), or another generated enum/struct (found via
// `generated_names`) -- `dependency` is that referenced TypeDef, used
// both for topological ordering (struct-to-struct) and for computing
// per-file #include sets.
struct resolved_field {
    std::string cpp_type;
    TypeDef dependency;
    std::string_view include;  // non-empty for projected types
};

resolved_field resolve_field_type(TypeSig const& sig, std::map<TypeDef, std::string> const& generated_names,
                                  type_map const& types) {
    if (auto const* prim = primitive_name(sig.element_type())) {
        return {prim, {}, {}};
    }
    if (auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type());
        ref && ref->type() != TypeDefOrRef::TypeSpec) {
        if (auto const resolved = md::find(*ref)) {
            if (auto const* projection = project_type(resolved, types)) {
                return {std::string{projection->cpp_name}, {}, projection->include};
            }
            if (auto const it = generated_names.find(resolved); it != generated_names.end()) {
                return {it->second, resolved, {}};
            }
        }
    }
    return {"void*", {}, {}};  // unexpected field shape (array/generic/unresolved) -- placeholder
}

// A struct as analysis sees it: its TypeDef, and each field with where its
// type came from. What a writer gets is the struct_info made from it.
struct struct_source {
    TypeDef type;
    std::vector<std::pair<std::string, resolved_field>> fields;  // camelCase name, type
};

struct_source read_struct(TypeDef const& type, std::map<TypeDef, std::string> const& generated_names,
                          type_map const& types) {
    struct_source source{type, {}};
    for (auto&& field : type.FieldList()) {
        source.fields.emplace_back(member_name(field.Name()),
                                   resolve_field_type(field.Signature().Type(), generated_names, types));
    }
    return source;
}

// The structs a struct's fields hold, which have to be declared before it.
// Enums don't take part: they live in a file of their own and depend on
// nothing.
void struct_dependencies(struct_source const& s, auto&& follow) {
    for (auto&& [name, field] : s.fields) {
        if (field.dependency && get_category(field.dependency) == category::struct_type) {
            follow(field.dependency);
        }
    }
}

struct_info analyze_struct(struct_source const& source) {
    auto const& type = source.type;
    struct_info info;
    info.name = std::string{type.TypeName()};
    info.winrt_name = std::format("{}::{}", winrt_namespace(type.TypeNamespace()), type.TypeName());
    info.projection_header = winrt_include(type.TypeNamespace());

    int next = 1;
    for (auto&& [name, field] : source.fields) {
        info.fields.push_back({name, field.cpp_type});
        if (!info.probe_complete) {
            continue;
        }

        if (!field.include.empty()) {
            info.probe_complete = false;
            continue;
        }

        std::string init;
        std::string check;
        if (field.dependency) {
            if (get_category(field.dependency) != category::enum_type) {
                info.probe_complete = false;
                continue;
            }
            auto const winrt_enum = std::format("{}::{}", winrt_namespace(field.dependency.TypeNamespace()),
                                                field.dependency.TypeName());
            init = std::format("static_cast<{}>({})", winrt_enum, next);
            check = std::format("v.{} == static_cast<{}>({})", name, field.cpp_type, next);
        } else {
            init = std::to_string(next);
            check = std::format("v.{} == {}", name, next);
        }

        if (!info.probe_init.empty()) {
            info.probe_init += ", ";
            info.probe_check += " && ";
        }
        info.probe_init += init;
        info.probe_check += check;
        ++next;
    }
    if (!info.probe_complete) {
        info.probe_init.clear();
        info.probe_check.clear();
    }
    return info;
}

void write_enums_file(std::filesystem::path const& path, std::vector<enum_info> const& enums) {
    auto out = open_output(path);

    // <stdint.h>, not <cstdint>: the emitted underlying types and field
    // types are unqualified (int32_t, not std::int32_t), and only the C
    // header is guaranteed to declare those in the global namespace.
    std::print(out, R"({}#pragma once

#include <stdint.h>

namespace wxl {{

)",
               banner);

    for (auto&& info : enums) {
        // Every enumerator carries its value: with some of them left out by a
        // profile, the rest must not move.
        std::print(out, "enum class {} : {}\n{{\n", info.name, info.underlying);
        for (auto&& value : info.values) {
            std::print(out, "    {} = {},\n", value.name, value.value);
        }
        std::print(out, "}};\n\n");
    }

    std::print(out, "}} // namespace wxl\n");
}

void write_structs_file(std::filesystem::path const& path, model const& m,
                        struct_file const& file) {
    auto out = open_output(path);

    // <stdint.h> rather than <cstdint>, for the same reason as in
    // write_enums_file() above: the emitted field types are unqualified. Only
    // where a field is one of them: a file of doubles and enums has no use for it.
    bool const fixed_width = std::ranges::any_of(file.structs, [&m](size_t index) {
        return std::ranges::any_of(m.structs[index].fields, [](auto const& field) {
            return field.cpp_type.ends_with("_t") && field.cpp_type.find("int") != std::string::npos;
        });
    });
    std::print(out, "{}#pragma once\n\n", banner);
    if (fixed_width) {
        std::print(out, "#include <stdint.h>\n");
    }

    write_includes(out, file.includes);
    std::print(out, "\nnamespace wxl {{\n\n");

    for (auto&& index : file.structs) {
        auto const& s = m.structs[index];
        std::print(out, "struct {}\n{{\n", s.name);
        for (auto&& field : s.fields) {
            std::print(out, "    {} {}{{}};\n", field.cpp_type, field.name);
        }
        std::print(out, "}};\n\n");
    }

    std::print(out, "}} // namespace wxl\n");
}

void write_struct_conversions(std::filesystem::path const& path,
                              std::vector<struct_info> const& structs) {
    auto out = open_output(path);

    std::set<std::string> includes{"Structs.h", "../impl/conversions.h"};
    for (auto&& s : structs) {
        includes.insert(s.projection_header);
    }

    std::print(out, R"({}#pragma once

)",
               banner);
    write_includes(out, includes);

    std::print(out, "\nnamespace wxl::impl {{\n");

    for (auto&& s : structs) {
        std::print(out, R"(
inline {0} to_winrt({1} const& value) {{
    return std::bit_cast<{0}>(value);
}}

inline {1} from_winrt({0} const& value) {{
    return std::bit_cast<{1}>(value);
}}
)",
                   s.winrt_name, s.name);

        if (s.probe_complete) {
            std::print(out, "\nstatic_assert(mirrors<{0}>({1}{{{2}}}, []({0} v) {{ return {3}; }}));\n",
                       s.name, s.winrt_name, s.probe_init, s.probe_check);
        } else {
            std::print(out,
                       "\nstatic_assert(sizeof({0}) == sizeof({1}) && alignof({0}) == alignof({1}));\n",
                       s.name, s.winrt_name);
        }
    }

    std::print(out, "\n}}  // namespace wxl::impl\n");
}

}  // namespace

std::vector<std::pair<std::string, std::string>> enum_members(
    TypeDef const& type, std::map<TypeDef, std::set<std::string>> const& kept) {
    std::vector<std::pair<std::string, std::string>> values;
    for (auto&& field : literals_of(type, kept).literals) {
        values.emplace_back(member_name(field.Name()), std::string{field.Name()});
    }
    return values;
}

void analyze_enums_and_structs(type_kinds const& kinds, closure const& cl, type_map const& types,
                               model& m) {
    std::vector<TypeDef> named;
    named.insert(named.end(), kinds.enums.begin(), kinds.enums.end());
    named.insert(named.end(), kinds.structs.begin(), kinds.structs.end());
    auto const generated_names = build_name_registry(named);

    // Grouping only slices the closure's dependency order per namespace; no
    // re-sorting here, the order the closure produced is the order emitted.
    std::map<std::string, std::vector<enum_info>> enums_by_namespace;
    for (auto&& type : kinds.enums) {
        enums_by_namespace[std::string(type.TypeNamespace())].push_back(
            analyze_enum(type, cl.members));
    }
    for (auto&& [ns, enums] : enums_by_namespace) {
        m.enum_files.push_back({ns, std::move(enums)});
    }

    std::vector<struct_source> sources;
    sources.reserve(kinds.structs.size());
    for (auto&& type : kinds.structs) {
        if (project_type(type, types)) {
            continue;  // wxl::geometry already provides it
        }
        if (!mirrors_abi(type, types)) {
            // A field wxl cannot mirror -- a String, today. Generating the
            // struct anyway would produce a bit_cast between two layouts
            // that only look alike, so it is dropped and said so; every
            // member naming it is dropped with it, and reported there.
            m.dropped_structs.push_back(full_name(type));
            continue;
        }
        sources.push_back(read_struct(type, generated_names, types));
    }
    auto const sorted = topological_sort(
        std::move(sources), [](struct_source const& s, auto&& follow) { struct_dependencies(s, follow); });

    // Per-namespace-file #include sets: any field whose resolved type
    // came from a *different* file than the struct itself needs an
    // explicit #include -- enums always live in a separate file from
    // structs (even within the same source namespace), structs only need
    // an #include when the dependency is in another namespace
    // (same-namespace struct deps are already ordered earlier in the same
    // file by the topological sort above), and a projected type needs the
    // header wxl keeps it in.
    std::map<std::string, struct_file> files;
    for (auto&& source : sorted) {
        std::string const ns{source.type.TypeNamespace()};
        auto& file = files[ns];
        file.ns = ns;
        file.structs.push_back(m.structs.size());
        m.structs.push_back(analyze_struct(source));

        for (auto&& [name, field] : source.fields) {
            if (!field.include.empty()) {
                file.includes.insert(std::string{field.include});
            }
            if (!field.dependency) {
                continue;
            }
            std::string const dep_ns{field.dependency.TypeNamespace()};
            auto const dep_cat = get_category(field.dependency);
            if (dep_cat == category::enum_type) {
                file.includes.insert(dep_ns + ".Enums.h");
            } else if (dep_cat == category::struct_type && dep_ns != ns) {
                file.includes.insert(dep_ns + ".Structs.h");
            }
        }
    }
    for (auto&& [ns, file] : files) {
        m.struct_files.push_back(std::move(file));
    }
}

void write_enums_and_structs(output const& out, model const& m, emitted& em) {
    std::vector<std::string> enum_files;
    for (auto&& file : m.enum_files) {
        std::string const filename = file.ns + ".Enums.h";
        auto const path = out.dir / filename;
        write_enums_file(path, file.enums);
        em.add(path);
        std::print("generated {} ({} enums)\n", path.string(), file.enums.size());
        enum_files.push_back(filename);
    }

    std::vector<std::string> struct_files;
    for (auto&& file : m.struct_files) {
        std::string const filename = file.ns + ".Structs.h";
        auto const path = out.dir / filename;
        write_structs_file(path, m, file);
        em.add(path);
        std::print("generated {} ({} structs)\n", path.string(), file.structs.size());
        struct_files.push_back(filename);
    }

    auto const enums_umbrella = out.dir / "Enums.h";
    write_umbrella_file(enums_umbrella, enum_files);
    em.add(enums_umbrella);
    std::print("generated {}\n", enums_umbrella.string());

    auto const structs_umbrella = out.dir / "Structs.h";
    write_umbrella_file(structs_umbrella, struct_files);
    em.add(structs_umbrella);
    std::print("generated {}\n", structs_umbrella.string());

    auto const struct_conversions = out.dir / "Structs.impl.h";
    write_struct_conversions(struct_conversions, m.structs);
    em.add(struct_conversions);
    std::print("generated {}\n", struct_conversions.string());

    if (!m.dropped_structs.empty()) {
        std::print("\nstructs dropped -- a field wxl cannot mirror ({}):\n",
                   m.dropped_structs.size());
        for (auto&& name : m.dropped_structs) {
            std::print("  {}\n", name);
        }
    }
}

}  // namespace gen
