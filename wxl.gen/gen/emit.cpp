module;

#include <format>
#include <print>

module wxl.gen;

import std;
import wxl.core;

using namespace md;

namespace gen {

std::ofstream open_output(std::filesystem::path const& path) {
    std::ofstream out{path};
    if (!out) {
        throw std::runtime_error(std::format("cannot open for writing: {}", path.string()));
    }
    return out;
}

void Emitted::add(std::filesystem::path const& path, std::string_view target) {
    files_.push_back(path.filename().string());
    targets_.emplace_back(target);
}

std::vector<std::string> Emitted::files_of(std::string_view target) const {
    std::vector<std::string> mine;
    for (std::size_t at = 0; at != files_.size(); ++at) {
        if (targets_[at] == target) {
            mine.push_back(files_[at]);
        }
    }
    return mine;
}

std::vector<std::string> Emitted::targets() const {
    std::vector<std::string> named;
    for (auto&& target : targets_) {
        if (std::find(named.begin(), named.end(), target) == named.end()) {
            named.push_back(target);
        }
    }
    return named;
}

std::string full_name(TypeDef const& type) {
    return std::format("{}.{}", type.TypeNamespace(), type.TypeName());
}

std::map<TypeDef, std::string> build_name_registry(std::vector<TypeDef> const& types) {
    std::map<TypeDef, std::string> generated_names;
    std::map<std::string, TypeDef> name_owner;

    for (auto&& type : types) {
        std::string name{type.TypeName()};
        if (auto const it = name_owner.find(name); it != name_owner.end() && it->second != type) {
            std::print(stderr, "warning: name collision in flat wxl namespace: '{}' ({} vs {})\n",
                       name, it->second.TypeNamespace(), type.TypeNamespace());
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
        "private",  "public",  "return", "short",  "signed",   "sizeof", "static",
        "struct",   "switch",  "template", "this", "throw",    "true",   "try",
        "typedef",  "typename", "union", "unsigned", "using",  "virtual", "void",
        "volatile", "while",
    };
    if (std::find(std::begin(keywords), std::end(keywords), name) != std::end(keywords)) {
        name += '_';
    }
    return name;
}

std::string interface_field_name(std::string_view interface_name) {
    // Drop the WinRT interface prefix: IButtonBase -> ButtonBase.
    if (interface_name.size() > 1 && interface_name[0] == 'I' &&
        std::isupper(static_cast<unsigned char>(interface_name[1]))) {
        interface_name.remove_prefix(1);
    }
    return member_name(interface_name) + '_';
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

std::string winrt_include(std::string_view metadata_namespace) {
    return std::format("<winrt/{}.h>", metadata_namespace);
}

}  // namespace gen
