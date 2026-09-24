#include <format>
#include <ostream>
#include <print>

#include "wxl.gen.h"

import std;

// Enums and structs, emitted flat into `namespace wxl`, grouped into one
// file pair per *source* WinRT namespace (Microsoft.UI.Xaml.Enums.h /
// Microsoft.UI.Xaml.Structs.h, ...), plus umbrella files (Enums.h /
// Structs.h) that #include every per-namespace file in turn. File and type
// names follow WinUI/WinRT's own PascalCase convention throughout.
//
// AI note: types wxl already owns a better equivalent of (Size/Point/Rect
// -> wxl::geometry) are *not* emitted here at all -- see gen/projection.h;
// they are skipped as definitions and referred to by their projected name
// wherever a field mentions them.

using namespace md;

namespace gen {
namespace {

// WinRT enum literals and struct fields only ever carry a primitive
// ElementType, another enum/struct (TypeDefOrRef), or -- unseen so far
// in practice -- something this generator doesn't understand yet.
// Returns nullptr for anything that isn't a primitive; the caller
// resolves those via the TypeDefOrRef alternative of TypeSig::Type()
// instead.
char const* primitive_cpp_type(ElementType et) {
    switch (et) {
        case ElementType::Boolean:
            return "bool";
        case ElementType::Char:
            return "char16_t";
        case ElementType::I1:
            return "int8_t";
        case ElementType::U1:
            return "uint8_t";
        case ElementType::I2:
            return "int16_t";
        case ElementType::U2:
            return "uint16_t";
        case ElementType::I4:
            return "int32_t";
        case ElementType::U4:
            return "uint32_t";
        case ElementType::I8:
            return "int64_t";
        case ElementType::U8:
            return "uint64_t";
        case ElementType::R4:
            return "float";
        case ElementType::R8:
            return "double";
        default:
            return nullptr;
    }
}

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

struct enum_info {
    TypeDef type;
    char const* underlying = "int32_t";
    std::vector<Field> literals;
};

// `kept` is what the walk recorded as the enum's members: the enumerators the
// profiles kept -- all of them for an enum they never name.
enum_info analyze_enum(TypeDef const& type, std::map<TypeDef, std::set<std::string>> const& kept) {
    enum_info info{type};
    auto const values = kept.find(type);
    for (auto&& field : type.FieldList()) {
        if (field.Flags().Literal()) {
            if (values != kept.end() && values->second.contains(std::string{field.Name()})) {
                info.literals.push_back(field);
            }
        } else if (field.Signature().Type().element_type() == ElementType::U4) {
            // The non-literal "value__" backing field's own type is the
            // enum's real underlying type -- WinRT enums are Int32
            // unless [FlagsAttribute] makes them UInt32.
            info.underlying = "uint32_t";
        }
    }
    return info;
}

void write_enums_file(std::filesystem::path const& path, std::vector<TypeDef> const& enums,
                      std::map<TypeDef, std::set<std::string>> const& kept) {
    auto out = open_output(path);

    // <stdint.h>, not <cstdint>: the emitted underlying types and field
    // types are unqualified (int32_t, not std::int32_t), and only the C
    // header is guaranteed to declare those in the global namespace.
    std::print(out, R"({}#pragma once

#include <stdint.h>

namespace wxl {{

)",
               banner);

    for (auto&& type : enums) {
        auto const info = analyze_enum(type, kept);
        // Every enumerator carries its value: with some of them left out by a
        // profile, the rest must not move.
        std::print(out, "enum class {} : {}\n{{\n", type.TypeName(), info.underlying);
        for (auto&& literal : info.literals) {
            std::print(out, "    {} = {},\n", literal.Name(), format_constant(literal.Constant()));
        }
        std::print(out, "}};\n\n");
    }

    std::print(out, "}} // namespace wxl\n");
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

resolved_field resolve_field_type(TypeSig const& sig,
                                  std::map<TypeDef, std::string> const& generated_names) {
    if (auto const* prim = primitive_cpp_type(sig.element_type())) {
        return {prim, {}, {}};
    }
    if (auto const* ref = std::get_if<coded_index<TypeDefOrRef>>(&sig.Type());
        ref && ref->type() != TypeDefOrRef::TypeSpec) {
        if (auto const resolved = md::find(*ref)) {
            if (auto const* projection = project_type(resolved)) {
                return {std::string{projection->cpp_name}, {}, projection->include};
            }
            if (auto const it = generated_names.find(resolved); it != generated_names.end()) {
                return {it->second, resolved, {}};
            }
        }
    }
    return {"void*", {}, {}};  // unexpected field shape (array/generic/unresolved) -- placeholder
}

// A field is a member, so wxl spells it camelCase where the WinRT original
// keeps the metadata PascalCase.
struct field_info {
    std::string name;
    resolved_field type;
};

struct struct_info {
    TypeDef type;
    std::vector<field_info> fields;
};

struct_info analyze_struct(TypeDef const& type,
                           std::map<TypeDef, std::string> const& generated_names) {
    struct_info info{type};
    for (auto&& field : type.FieldList()) {
        info.fields.push_back({member_name(field.Name()),
                               resolve_field_type(field.Signature().Type(), generated_names)});
    }
    return info;
}

// Orders `structs` so that any struct referenced by another struct's
// field is emitted first -- C++ requires a field's type to be complete
// at the point of use, so declaration order has to follow dependency
// order. Enum dependencies don't participate (enums are always
// self-contained, emitted into a separate file, and never depend on
// anything themselves). DFS-based; WinRT structs are simple flat
// aggregates in practice, so cycles aren't expected -- the `visiting`
// guard just avoids infinite recursion if metadata ever surprised us.
std::vector<struct_info> topological_sort(std::vector<struct_info> structs) {
    std::map<TypeDef, struct_info const*> by_type;
    for (auto&& s : structs) {
        by_type[s.type] = &s;
    }

    std::set<TypeDef> done;
    std::set<TypeDef> visiting;
    std::vector<struct_info> order;
    order.reserve(structs.size());

    std::function<void(struct_info const&)> visit = [&](struct_info const& s) {
        if (done.count(s.type) || !visiting.insert(s.type).second) {
            return;
        }
        for (auto&& field : s.fields) {
            if (field.type.dependency &&
                get_category(field.type.dependency) == category::struct_type) {
                if (auto const it = by_type.find(field.type.dependency); it != by_type.end()) {
                    visit(*it->second);
                }
            }
        }
        done.insert(s.type);
        order.push_back(s);
    };

    for (auto&& s : structs) {
        visit(s);
    }
    return order;
}

void write_structs_file(std::filesystem::path const& path, std::vector<struct_info> const& structs,
                        std::set<std::string> const& includes) {
    auto out = open_output(path);

    // <stdint.h> rather than <cstdint>, for the same reason as in
    // write_enums_file() above: the emitted field types are unqualified.
    std::print(out, R"({}#pragma once

#include <stdint.h>
)",
               banner);

    for (auto&& include : includes) {
        // Projections name standard headers (<chrono>); everything else is
        // a generated file next door.
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }
    std::print(out, "\nnamespace wxl {{\n\n");

    for (auto&& s : structs) {
        std::print(out, "struct {}\n{{\n", s.type.TypeName());
        for (auto&& field : s.fields) {
            std::print(out, "    {} {}{{}};\n", field.type.cpp_type, field.name);
        }
        std::print(out, "}};\n\n");
    }

    std::print(out, "}} // namespace wxl\n");
}

// How one field crosses the boundary. A struct is converted field by field
// rather than cast whole: the wxl side substitutes its own enums and its own
// geometry types, so the two layouts agree only by accident, and relying on
// that would make every future projection a silent hazard.
// One distinct value per field, so that the assert catches a reordering and
// not merely a resize: the WinRT struct is built from these, bit-cast across
// and read back under the wxl names. A field that is neither a primitive nor
// an enum has no such literal, and `complete` says so -- that pair falls back
// to an assert on size and alignment alone.
struct layout_probe {
    bool complete = true;
    std::string winrt_init;
    std::string wxl_check;
};

layout_probe build_probe(struct_info const& s) {
    layout_probe probe;
    int next = 1;
    for (auto&& field : s.fields) {
        if (!field.type.include.empty()) {
            return {false, {}, {}};
        }

        std::string init;
        std::string check;
        if (field.type.dependency) {
            if (get_category(field.type.dependency) != category::enum_type) {
                return {false, {}, {}};
            }
            auto const winrt_enum =
                std::format("{}::{}", winrt_namespace(field.type.dependency.TypeNamespace()),
                            field.type.dependency.TypeName());
            init = std::format("static_cast<{}>({})", winrt_enum, next);
            check = std::format("v.{} == static_cast<{}>({})", field.name, field.type.cpp_type,
                                next);
        } else {
            init = std::to_string(next);
            check = std::format("v.{} == {}", field.name, next);
        }

        if (!probe.winrt_init.empty()) {
            probe.winrt_init += ", ";
            probe.wxl_check += " && ";
        }
        probe.winrt_init += init;
        probe.wxl_check += check;
        ++next;
    }
    return probe;
}

void write_struct_conversions(std::filesystem::path const& path,
                              std::vector<struct_info> const& structs) {
    auto out = open_output(path);

    std::set<std::string> includes{"Structs.h", "../impl/conversions.h"};
    for (auto&& s : structs) {
        includes.insert(std::format("<winrt/{}.h>", s.type.TypeNamespace()));
    }

    std::print(out, R"({}// Conversions between the generated structs and their WinRT originals,
// joining the impl::to_winrt / impl::from_winrt overload sets that
// impl/conversions.h opens. Private: generated .cpp files include this,
// consuming code never does.
//
// A wxl struct is the ABI struct -- it is generated from the same metadata,
// field for field -- so crossing is a bit_cast and the assert beside each
// pair is what keeps that true.
#pragma once

)",
               banner);
    for (auto&& include : includes) {
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }

    std::print(out, "\nnamespace wxl::impl {{\n");

    for (auto&& s : structs) {
        auto const winrt_name =
            std::format("{}::{}", winrt_namespace(s.type.TypeNamespace()), s.type.TypeName());

        std::print(out, R"(
inline {0} to_winrt({1} const& value) {{
    return std::bit_cast<{0}>(value);
}}

inline {1} from_winrt({0} const& value) {{
    return std::bit_cast<{1}>(value);
}}
)",
                   winrt_name, s.type.TypeName());

        auto const probe = build_probe(s);
        if (probe.complete) {
            std::print(out, "\nstatic_assert(mirrors<{0}>({1}{{{2}}}, []({0} v) {{ return {3}; }}));\n",
                       s.type.TypeName(), winrt_name, probe.winrt_init, probe.wxl_check);
        } else {
            std::print(out,
                       "\nstatic_assert(sizeof({0}) == sizeof({1}) && alignof({0}) == alignof({1}));\n",
                       s.type.TypeName(), winrt_name);
        }
    }

    std::print(out, "\n}}  // namespace wxl::impl\n");
}

void write_umbrella_file(std::filesystem::path const& path, std::vector<std::string> const& files) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n", banner);
    for (auto&& file : files) {
        std::print(out, "#include \"{}\"\n", file);
    }
}

}  // namespace

std::vector<std::pair<std::string, std::string>> enum_members(
    TypeDef const& type, std::map<TypeDef, std::set<std::string>> const& kept) {
    std::vector<std::pair<std::string, std::string>> values;
    for (auto&& field : analyze_enum(type, kept).literals) {
        values.emplace_back(member_name(field.Name()), std::string{field.Name()});
    }
    return values;
}

void write_enums_and_structs(Output const& out, Model const& model, Emitted& emitted) {
    std::vector<TypeDef> named;
    named.insert(named.end(), model.enums.begin(), model.enums.end());
    named.insert(named.end(), model.structs.begin(), model.structs.end());
    auto const generated_names = build_name_registry(named);

    // Grouping only slices the model's dependency order per namespace; no
    // re-sorting here, the order the closure produced is the order emitted.
    std::map<std::string, std::vector<TypeDef>> enums_by_namespace;
    for (auto&& type : model.enums) {
        enums_by_namespace[std::string(type.TypeNamespace())].push_back(type);
    }

    std::vector<struct_info> struct_infos;
    std::vector<std::string> dropped;
    struct_infos.reserve(model.structs.size());
    for (auto&& type : model.structs) {
        if (project_type(type)) {
            continue;  // wxl::geometry already provides it
        }
        if (!mirrors_abi(type)) {
            // A field wxl cannot mirror -- a String, today. Generating the
            // struct anyway would produce a bit_cast between two layouts
            // that only look alike, so it is dropped and said so; every
            // member naming it is dropped with it, and reported there.
            dropped.push_back(full_name(type));
            continue;
        }
        struct_infos.push_back(analyze_struct(type, generated_names));
    }
    auto const sorted_structs = topological_sort(std::move(struct_infos));

    std::map<std::string, std::vector<struct_info>> structs_by_namespace;
    for (auto&& s : sorted_structs) {
        structs_by_namespace[std::string(s.type.TypeNamespace())].push_back(s);
    }

    // Per-namespace-file #include sets: any field whose resolved type
    // came from a *different* file than the struct itself needs an
    // explicit #include -- enums always live in a separate file from
    // structs (even within the same source namespace), structs only need
    // an #include when the dependency is in another namespace
    // (same-namespace struct deps are already ordered earlier in the same
    // file by the topological sort above), and a projected type needs the
    // header wxl keeps it in.
    std::map<std::string, std::set<std::string>> struct_file_includes;
    for (auto&& [ns, infos] : structs_by_namespace) {
        std::set<std::string> includes;
        for (auto&& s : infos) {
            for (auto&& field : s.fields) {
                if (!field.type.include.empty()) {
                    includes.insert(std::string{field.type.include});
                }
                if (!field.type.dependency) {
                    continue;
                }
                std::string const dep_ns{field.type.dependency.TypeNamespace()};
                auto const dep_cat = get_category(field.type.dependency);
                if (dep_cat == category::enum_type) {
                    includes.insert(dep_ns + ".Enums.h");
                } else if (dep_cat == category::struct_type && dep_ns != ns) {
                    includes.insert(dep_ns + ".Structs.h");
                }
            }
        }
        struct_file_includes[ns] = std::move(includes);
    }

    std::vector<std::string> enum_files;
    for (auto&& [ns, types] : enums_by_namespace) {
        std::string const filename = ns + ".Enums.h";
        auto const path = out.dir / filename;
        write_enums_file(path, types, model.members);
        emitted.add(path);
        std::print("wrote {} ({} enums)\n", path.string(), types.size());
        enum_files.push_back(filename);
    }

    std::vector<std::string> struct_files;
    for (auto&& [ns, infos] : structs_by_namespace) {
        std::string const filename = ns + ".Structs.h";
        auto const path = out.dir / filename;
        write_structs_file(path, infos, struct_file_includes[ns]);
        emitted.add(path);
        std::print("wrote {} ({} structs)\n", path.string(), infos.size());
        struct_files.push_back(filename);
    }

    auto const enums_umbrella = out.dir / "Enums.h";
    write_umbrella_file(enums_umbrella, enum_files);
    emitted.add(enums_umbrella);
    std::print("wrote {}\n", enums_umbrella.string());

    auto const structs_umbrella = out.dir / "Structs.h";
    write_umbrella_file(structs_umbrella, struct_files);
    emitted.add(structs_umbrella);
    std::print("wrote {}\n", structs_umbrella.string());

    auto const struct_conversions = out.dir / "Structs.impl.h";
    write_struct_conversions(struct_conversions, sorted_structs);
    emitted.add(struct_conversions);
    std::print("wrote {}\n", struct_conversions.string());

    if (!dropped.empty()) {
        std::print("\nstructs dropped -- a field wxl cannot mirror ({}):\n", dropped.size());
        for (auto&& name : dropped) {
            std::print("  {}\n", name);
        }
    }
}

}  // namespace gen
