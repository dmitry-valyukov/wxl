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
        throw std::runtime_error(
            std::format("cannot write {}", wxl::core::unicode::to_utf8(path_).chars()));
    }
}

output_file open_output(std::filesystem::path const& path) {
    return output_file{path};
}

namespace {

constexpr std::size_t doc_width = 100;

// Every character Doxygen reads as the start of a command, a tag, an entity or
// a link, with the backslash that makes it a character again.
std::string doxygen_text(std::string_view word) {
    std::string text;
    for (char const c : word) {
        if (std::string_view{"\\@#<>&$%\""}.contains(c)) {
            text += '\\';
        }
        text += c;
    }
    return text;
}

// Columns as a reader sees them: UTF-8 continuation bytes take none.
std::size_t columns(std::string_view text) {
    return static_cast<std::size_t>(
        std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

// A word Markdown reads as a list item, a rule or a heading underline when it
// starts a line: "-", "*", "+", "1.", "2)", "---", "===". Wrapping never
// starts a line with one.
bool opens_block(std::string_view word) {
    if (std::ranges::all_of(word, [](char c) { return c == '-' || c == '*' || c == '+' || c == '=' || c == '_'; })) {
        return true;
    }
    return word.size() > 1 && (word.back() == '.' || word.back() == ')') &&
           std::ranges::all_of(word.substr(0, word.size() - 1), [](char c) { return c >= '0' && c <= '9'; });
}

// A line of the comment. It never ends with a backslash: the compiler splices
// the next line onto such a line, whitespace after it or not, and the
// declaration below the comment would become comment too. An empty HTML
// comment, which Doxygen drops, stands after it instead.
void write_line(std::ostream& out, std::string_view line) {
    std::print(out, "{}{}\n", line, line.ends_with('\\') ? "<!---->" : "");
}

// One paragraph: `lead` -- a command and what it names -- and the text after it,
// word by word.
void write_paragraph(std::ostream& out, std::string_view indent, std::string_view lead, std::string_view text) {
    std::string line = std::format("{}///{}{}", indent, lead.empty() ? "" : " ", lead);
    bool bare = lead.empty();  // nothing after the slashes yet
    for (auto&& part : std::views::split(text, ' ')) {
        std::string_view const word{part.begin(), part.end()};
        if (word.empty()) {
            continue;
        }
        std::string const escaped = doxygen_text(word);
        if (!bare && columns(line) + 1 + columns(escaped) > doc_width && !opens_block(word)) {
            write_line(out, line);
            line = std::format("{}///", indent);
            bare = true;
        }
        line += ' ';
        line += escaped;
        bare = false;
    }
    write_line(out, line);
}

bool blank(std::string_view text) {
    return std::ranges::all_of(text, [](char c) { return c == ' '; });
}

}  // namespace

void write_doc(std::ostream& out, std::string_view indent, member_documentation const& doc,
               std::span<std::string_view const> params, bool has_result) {
    std::vector<std::pair<std::string, std::string_view>> blocks;  // lead, text
    if (!blank(doc.deprecated)) {
        blocks.emplace_back("@deprecated", doc.deprecated);
    }
    for (std::string_view const name : params) {
        auto const described = std::ranges::find(doc.params, name, &std::pair<std::string, std::string>::first);
        if (described != doc.params.end() && !blank(described->second)) {
            blocks.emplace_back(std::format("@param {}", name), described->second);
        }
    }
    if (has_result && !blank(doc.returns)) {
        blocks.emplace_back("@return", doc.returns);
    }

    bool const summary = !blank(doc.summary);
    if (summary) {
        write_paragraph(out, indent, {}, doc.summary);
    }
    if (summary && !blocks.empty()) {
        std::print(out, "{}///\n", indent);
    }
    for (auto&& [lead, text] : blocks) {
        write_paragraph(out, indent, lead, text);
    }
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
    files_.push_back(path.filename());
    targets_.emplace_back(target);
}

std::vector<std::filesystem::path> emitted::files_of(std::string_view target) const {
    std::vector<std::filesystem::path> mine;
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
    // Control.Template, ElementTheme.Default, ScrollBarVisibility.Auto -- or
    // on an alternative token, which the language reads as an operator:
    // CanvasComposite.Xor. A trailing underscore is the escape that keeps the
    // name recognisable as the member it stands for. Every keyword of one
    // word is here: a name of several words keeps a capital inside it and
    // cannot land on one.
    static constexpr std::string_view keywords[] = {
        "alignas",  "alignof",  "and",       "asm",      "auto",     "bitand",    "bitor",
        "bool",     "break",    "case",      "catch",    "char",     "class",     "compl",
        "concept",  "const",    "consteval", "constexpr", "constinit", "continue", "decltype",
        "default",  "delete",   "do",        "double",   "else",     "enum",      "explicit",
        "export",   "extern",   "false",     "float",    "for",      "friend",    "goto",
        "if",       "inline",   "int",       "long",     "mutable",  "namespace", "new",
        "noexcept", "not",      "nullptr",   "operator", "or",       "private",   "protected",
        "public",   "register", "requires",  "return",   "short",    "signed",    "sizeof",
        "static",   "struct",   "switch",    "template", "this",     "throw",     "true",
        "try",      "typedef",  "typeid",    "typename", "union",    "unsigned",  "using",
        "virtual",  "void",     "volatile",  "while",    "xor",
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
