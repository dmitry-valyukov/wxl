#include <format>
#include <print>

#include "wxl.gen.h"

import std;
import wxl.core;

using namespace md;

namespace gen {

output_file::output_file(std::filesystem::path path) : path_(std::move(path)) {}

output_file::~output_file() noexcept(false) {
    if (std::uncaught_exceptions() > 0) {
        return;
    }

    std::string const text = std::move(text_).str();
    if (std::ifstream existing{path_}) {
        std::string const old{std::istreambuf_iterator<char>{existing}, {}};
        if (old == text) {
            return;
        }
    }

    std::ofstream file{path_};
    file.write(text.data(), static_cast<std::streamsize>(text.size()));
    if (!file) {
        throw std::runtime_error(std::format("cannot write {}", path_.string()));
    }
}

output_file open_output(std::filesystem::path const& path) {
    return output_file{path};
}

void write_includes(std::ostream& out, std::set<std::string> const& includes) {
    for (auto&& include : includes) {
        if (include.starts_with('<')) {
            std::print(out, "#include {}\n", include);
        } else {
            std::print(out, "#include \"{}\"\n", include);
        }
    }
}

void write_umbrella_file(std::filesystem::path const& path, std::vector<std::string> const& files) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n", banner);
    for (auto&& file : files) {
        std::print(out, "#include \"{}\"\n", file);
    }
}

void emitted::add(std::filesystem::path const& path, std::string_view target) {
    files_.push_back(path.filename().string());
    targets_.emplace_back(target);
}

std::vector<std::string> emitted::files_of(std::string_view target) const {
    std::vector<std::string> mine;
    for (std::size_t at = 0; at != files_.size(); ++at) {
        if (targets_[at] == target) {
            mine.push_back(files_[at]);
        }
    }
    return mine;
}

std::vector<std::string> emitted::targets() const {
    std::vector<std::string> named;
    for (auto&& target : targets_) {
        if (std::find(named.begin(), named.end(), target) == named.end()) {
            named.push_back(target);
        }
    }
    return named;
}

std::map<TypeDef, std::string> build_name_registry(std::vector<TypeDef> const& types) {
    std::map<TypeDef, std::string> generated_names;
    std::map<std::string, TypeDef> name_owner;

    for (auto&& type : types) {
        std::string name{type.TypeName()};
        if (auto const it = name_owner.find(name); it != name_owner.end() && it->second != type) {
            throw std::runtime_error(
                std::format("two types map to the same flat wxl name '{}': {} and {}", name,
                            full_name(it->second), full_name(type)));
        }
        generated_names[type] = name;
        name_owner[std::move(name)] = type;
    }
    return generated_names;
}

std::string member_name(std::string_view metadata_name) {
    std::string name{metadata_name};

    // Lower-case the leading run of capitals, so acronyms read as one word:
    // Content -> content, UIElement -> uiElement, ID -> id. When the run is
    // followed by a lowercase letter, its last capital already starts the
    // next word (UIElement: "UI" + "Element") and stays capitalised.
    size_t run = 0;
    while (run < name.size() && std::isupper(static_cast<unsigned char>(name[run]))) {
        ++run;
    }
    if (run > 1 && run < name.size()) {
        --run;
    }
    for (size_t i = 0; i < run; ++i) {
        // ASCII and nothing else, which is what a WinRT name is -- and unlike
        // std::tolower it cannot be told otherwise by a locale.
        name[i] = wxl::core::to_ascii_lower(name[i]);
    }

    // A handful of WinRT names land on a C++ keyword once lower-cased --
    // Control.Template, ElementTheme.Default, ScrollBarVisibility.Auto. A
    // trailing underscore is the escape that keeps the name recognisable as
    // the member it stands for.
    static constexpr std::string_view keywords[] = {
        "auto",     "bool",    "break",  "case",   "catch",    "char",   "class",
        "const",    "default", "delete", "double", "else",     "enum",   "explicit",
        "export",   "extern",  "false",  "float",  "for",      "friend", "goto",
        "if",       "inline",  "int",    "long",   "mutable",  "new",    "operator",
        "private",  "public",  "register", "return", "short",  "signed",   "sizeof", "static",
        "struct",   "switch",  "template", "this", "throw",    "true",   "try",
        "typedef",  "typename", "union", "unsigned", "using",  "virtual", "void",
        "volatile", "while",
    };
    if (std::find(std::begin(keywords), std::end(keywords), name) != std::end(keywords)) {
        name += '_';
    }
    return name;
}

std::string_view without_interface_prefix(std::string_view interface_name) {
    if (interface_name.size() > 1 && interface_name[0] == 'I' && interface_name[1] >= 'A' &&
        interface_name[1] <= 'Z') {
        interface_name.remove_prefix(1);
    }
    return interface_name;
}

std::string interface_field_name(std::string_view interface_name) {
    return member_name(without_interface_prefix(interface_name)) + '_';
}

std::string winrt_namespace(std::string_view metadata_namespace) {
    std::string result{"winrt::"};
    for (auto&& c : metadata_namespace) {
        if (c == '.') {
            result += "::";
        } else {
            result.push_back(c);
        }
    }
    return result;
}

std::string winrt_type_name(std::string_view metadata_namespace, std::string_view name) {
    if (metadata_namespace == "Windows.Foundation.Numerics") {
        static constexpr std::pair<std::string_view, std::string_view> spelled[] = {
            {"Vector2", "float2"},      {"Vector3", "float3"},       {"Vector4", "float4"},
            {"Matrix3x2", "float3x2"},  {"Matrix4x4", "float4x4"},   {"Plane", "plane"},
            {"Quaternion", "quaternion"},
        };
        for (auto&& [metadata, cpp] : spelled) {
            if (name == metadata) {
                return std::format("{}::{}", winrt_namespace(metadata_namespace), cpp);
            }
        }
    }
    return std::format("{}::{}", winrt_namespace(metadata_namespace), name);
}

std::string winrt_include(std::string_view metadata_namespace) {
    return std::format("<winrt/{}.h>", metadata_namespace);
}

}  // namespace gen
