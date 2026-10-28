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
        if (doxygen_escaped.contains(c)) {
            text += '\\';
        }
        text += c;
    }
    return text;
}

// Text as HTML that Doxygen passes through untouched: what would start a tag,
// an entity or a command of Doxygen's, or end an attribute, as an entity. A
// command matters even there: \endhtmlonly would end the HTML early.
std::string html_text(std::string_view text) {
    std::string html;
    for (char const c : text) {
        switch (c) {
            case '&': html += "&amp;"; break;
            case '"': html += "&quot;"; break;
            case '<': html += "&lt;"; break;
            case '>': html += "&gt;"; break;
            case '@': html += "&#64;"; break;
            case '\\': html += "&#92;"; break;
            default: html += c; break;
        }
    }
    return html;
}

// Columns as a reader sees them: UTF-8 continuation bytes take none.
std::size_t columns(std::string_view text) {
    return static_cast<std::size_t>(
        std::ranges::count_if(text, [](char c) { return (static_cast<unsigned char>(c) & 0xC0) != 0x80; }));
}

// The space separators of Unicode in UTF-8, but the space, at which the text
// is split into words: U+00A0, U+1680, U+2000 to U+200A, U+202F, U+205F, U+3000.
constexpr std::string_view space_separators[] = {
    "\xC2\xA0", "\xE1\x9A\x80", "\xE2\x80\x80", "\xE2\x80\x81", "\xE2\x80\x82", "\xE2\x80\x83",
    "\xE2\x80\x84", "\xE2\x80\x85", "\xE2\x80\x86", "\xE2\x80\x87", "\xE2\x80\x88", "\xE2\x80\x89",
    "\xE2\x80\x8A", "\xE2\x80\xAF", "\xE2\x81\x9F", "\xE3\x80\x80"};

// The bytes of the space separators that end `text`.
std::size_t space_separators_ending(std::string_view text) {
    for (std::string_view const separator : space_separators) {
        if (text.ends_with(separator)) {
            return separator.size() + space_separators_ending(text.substr(0, text.size() - separator.size()));
        }
    }
    return 0;
}

// A word that opens a block when it starts a line: what Markdown reads as a
// list item, a rule or a heading underline -- "-", "*", "+", "1.", "2)", "---",
// "===" -- the lone "." Doxygen reads as the end of a list, and warns of with no
// list before, and three backticks or tildes, which open a code block. Wrapping
// never starts a line with one.
bool opens_block(std::string_view word) {
    if (word == "." || word.starts_with("```") || word.starts_with("~~~") ||
        std::ranges::all_of(word, [](char c) { return c == '-' || c == '*' || c == '+' || c == '=' || c == '_'; })) {
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

// A word of a paragraph as it is written -- text escaped for Doxygen, markup as
// the HTML or Markdown Doxygen renders -- whether it starts a line, and whether
// it is a block marker meant as one there: a list item's, or the dot that ends
// the list.
struct doc_word {
    std::string text;
    bool starts_line = false;
    bool marker = false;
};

// The words of a paragraph. A word is what stands between two spaces, and
// markup joins the word it touches: `t<sup>3</sup>.` is one word, never broken
// across lines. `html` words are for HTML Doxygen passes through as it is.
class doc_words {
public:
    explicit doc_words(bool html = false) noexcept : html_(html) {}

    bool html() const noexcept { return html_; }

    // Text of the description: a word at each space, each escaped.
    void text(std::string_view text) {
        bool first = true;
        for (auto&& part : std::views::split(text, ' ')) {
            if (!first) {
                open_ = false;
            }
            first = false;
            std::string_view const piece{part.begin(), part.end()};
            if (!piece.empty()) {
                word() += html_ ? html_text(piece) : doxygen_text(piece);
                word_has_text_ = true;
            }
        }
    }

    void markup(std::string_view markup) { word() += markup; }

    // What follows starts a line, and so does the markup before it in the same
    // word, unless the word has text already: an image does not part from the
    // emphasis mark it opens with. The space separators that would end the line
    // before go with it, so that none ends in white space and the no-break
    // spaces a page puts between a code and its icon still part them.
    void line_with_markup() {
        std::string moved;
        if (open_ && !word_has_text_) {
            moved = std::move(words_.back().text);
            words_.pop_back();
        }
        moved.insert(0, trailing_space());
        new_line();
        word() += moved;
    }

    // What follows is another word.
    void space() noexcept { open_ = false; }

    // What follows is another word, and it starts a line. The space separators
    // that would end the line before go: no line ends in white space.
    void new_line() {
        trailing_space();
        open_ = false;
        line_ = true;
    }

    // A list item starts here: a line, and the marker Markdown reads as one.
    void item() { marker("-"); }

    // A list ends here: a line of a dot, which Doxygen reads so.
    void list_end() { marker("."); }

    // A code span as it comes: one word, the spaces in it too, since inside a
    // span Doxygen would read a line starting with "- " as a list item and show
    // the guard of a line ending with a backslash. A backslash before it, which
    // Doxygen would take for the escape of its backtick, is parted from it by an
    // empty HTML comment, which Doxygen drops.
    void code(std::string_view span) {
        std::string& text = word();
        if (text.ends_with('\\')) {
            text += "<!---->";
        }
        text += span;
        word_has_text_ = true;
    }

    std::vector<doc_word> const& list() const noexcept { return words_; }

private:
    void marker(std::string_view text) {
        new_line();
        word() = text;
        words_.back().marker = true;
        open_ = false;
    }

    std::string& word() {
        if (!open_) {
            words_.push_back({{}, line_});
            open_ = true;
            line_ = false;
            word_has_text_ = false;
        }
        return words_.back().text;
    }

    // The space separators that end the line at the last word, taken off it. A
    // word of them alone goes, and then the one before it on the line ends it.
    std::string trailing_space() {
        std::string space;
        while (!words_.empty()) {
            doc_word& last = words_.back();
            std::size_t const kept = last.text.size() - space_separators_ending(last.text);
            if (kept == last.text.size()) {
                break;
            }
            space.insert(0, last.text, kept);
            if (kept > 0) {
                last.text.resize(kept);
                break;
            }
            bool const alone = last.starts_line;
            words_.pop_back();
            open_ = false;
            if (alone) {
                break;
            }
        }
        return space;
    }

    bool const html_;
    std::vector<doc_word> words_;
    bool open_ = false;           // the last word goes on: nothing has ended it
    bool line_ = false;           // the next word starts a line
    bool word_has_text_ = false;  // the last word holds text, not markup alone
};

// A value of an HTML attribute, what would end it or start an entity in it
// written as an entity.
std::string html_attribute(std::string_view value) {
    std::string text;
    for (char const c : value) {
        switch (c) {
            case '&': text += "&amp;"; break;
            case '"': text += "&quot;"; break;
            case '<': text += "&lt;"; break;
            case '>': text += "&gt;"; break;
            default: text += c; break;
        }
    }
    return text;
}

// A code span as written, for Doxygen to show so: Doxygen escapes what is in a
// span itself, and the characters stand as they are but where Doxygen would
// misread them. A % before a letter, an underscore, a ' or a character beyond
// ASCII would go, as the escape of a word, and is doubled. An odd run of
// backslashes before the closing backticks would escape what follows it as
// Doxygen reads the comment -- the command it ends the span's text with, or a
// backtick -- and a space stands after it. And two things would end the comment
// early: */, and a command before a {, which Doxygen takes for an alias with
// arguments even in a span and reads on to the }, leaving a */ in the text after
// the span as it is. There the span closes, after the * or before the {, and
// opens again, an empty HTML comment between, which Doxygen drops.
std::string code_span_text(std::string_view span) {
    std::size_t const ticks = span.find_first_not_of('`');
    std::string_view const marks = span.substr(0, ticks);
    std::string_view const inside = span.substr(ticks, span.size() - 2 * ticks);
    std::string const reopen = std::format("{}<!---->{}", marks, marks);
    auto const name_char = [](char c) {
        return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-';
    };
    std::string text {marks};
    for (std::size_t at = 0; at < inside.size(); ++at) {
        char const c = inside[at];
        char const next = at + 1 < inside.size() ? inside[at + 1] : '\0';
        if (c == '%' && ((next >= 'a' && next <= 'z') || (next >= 'A' && next <= 'Z') || next == '_' ||
                         next == '\'' || static_cast<unsigned char>(next) >= 0x80)) {
            text += '%';
        }
        if (c == '{') {
            std::size_t name = at;  // the command's name, \ or @ and a letter or _ first
            while (name > 0 && name_char(inside[name - 1])) {
                --name;
            }
            char const first = name < at ? inside[name] : '\0';
            if (name > 0 && (inside[name - 1] == '\\' || inside[name - 1] == '@') &&
                ((first >= 'a' && first <= 'z') || (first >= 'A' && first <= 'Z') || first == '_')) {
                text += reopen;
            }
        }
        text += c;
        if (c == '*' && next == '/') {
            text += reopen;
        }
    }
    std::size_t backslashes = 0;  // the run of them that ends the text inside
    while (backslashes < inside.size() && inside[inside.size() - 1 - backslashes] == '\\') {
        ++backslashes;
    }
    if (backslashes % 2 == 1) {
        text += ' ';
    }
    text += marks;
    return text;
}

// The words of `text`: bold, italic and code spans in the Markdown they came in,
// the rest as the HTML Doxygen renders, a list of definitions as a Markdown list;
// in HTML Doxygen passes through, a code span is text. An image starts a line,
// since after text it would end a summary of one line, Doxygen's brief, there; in
// `deprecated` a table is HTML Doxygen passes through, on one line, since Doxygen
// ends that section at a table of its own.
void add_words(doc_words& words, doc_text const& text, bool const deprecated) {
    using kind = doc_run::kind_t;
    for (auto&& run : text) {
        switch (run.kind) {
            case kind::text:
                words.text(run.text);
                break;
            case kind::code:
                if (words.html()) {
                    words.text(run.text);
                } else {
                    words.code(code_span_text(run.text));
                }
                break;
            case kind::bold:
            case kind::italic: {
                std::string_view const tag = run.kind == kind::bold ? "b" : "em";
                // Doxygen opens emphasis before no tag; an entity before it lets it.
                bool const tagged = !run.runs.empty() &&
                                    (run.runs.front().kind == kind::image || run.runs.front().kind == kind::line_break);
                words.markup(words.html() ? std::format("<{}>", tag) : run.text + (tagged ? "&zwnj;" : ""));
                add_words(words, run.runs, deprecated);
                words.markup(words.html() ? std::format("</{}>", tag) : run.text);
                break;
            }
            case kind::superscript:
            case kind::subscript: {
                std::string_view const tag = run.kind == kind::superscript ? "sup" : "sub";
                words.markup(std::format("<{}>", tag));
                add_words(words, run.runs, deprecated);
                words.markup(std::format("</{}>", tag));
                break;
            }
            case kind::line_break:
                words.markup("<br>");
                break;
            case kind::image:
                words.line_with_markup();
                words.markup(
                    std::format("<img src=\"{}\" alt=\"{}\">", html_attribute(run.url), html_attribute(run.text)));
                break;
            case kind::definitions:
                for (auto&& definition : run.runs) {
                    words.item();
                    words.markup(std::format("`{}`", definition.text));
                    words.space();
                    words.markup("—");
                    words.space();
                    add_words(words, definition.runs, deprecated);
                }
                // A line of a dot closes the list, which the text after it would go on.
                if (&run != &text.back()) {
                    words.list_end();
                }
                words.new_line();
                break;
            case kind::table: {
                doc_words html {deprecated};
                doc_words& cells = deprecated ? html : words;
                cells.new_line();
                cells.markup(deprecated ? "@htmlonly<table class=\"doxtable\">" : "<table>");
                for (auto&& row : run.runs) {
                    cells.new_line();
                    cells.markup("<tr>");
                    for (auto&& cell : row.runs) {
                        std::string_view const tag = cell.kind == kind::header_cell ? "th" : "td";
                        cells.markup(std::format("<{}>", tag));
                        add_words(cells, cell.runs, deprecated);
                        cells.markup(std::format("</{}>", tag));
                    }
                    cells.markup("</tr>");
                }
                cells.new_line();
                cells.markup(deprecated ? "</table>@endhtmlonly" : "</table>");
                if (deprecated) {
                    std::string line;
                    for (auto&& word : html.list()) {
                        line += line.empty() ? "" : " ";
                        line += word.text;
                    }
                    words.new_line();
                    words.markup(line);
                }
                words.space();
                break;
            }
            case kind::definition:
            case kind::row:
            case kind::header_cell:
            case kind::cell:
                break;  // written with the list and the table that hold them
        }
    }
}

// One paragraph: `lead` -- a command and what it names -- and the text after it,
// word by word. Text that has to start a line -- after the dot that closes a
// list -- and that would open a block there gets a zero-width non-joiner in
// front, which Doxygen writes out as it is.
void write_paragraph(std::ostream& out, std::string_view indent, std::string_view lead, doc_text const& text) {
    doc_words words;
    add_words(words, text, lead == "@deprecated");
    std::string line = std::format("{}///{}{}", indent, lead.empty() ? "" : " ", lead);
    bool bare = lead.empty();  // nothing after the slashes yet
    for (auto&& word : words.list()) {
        if (!bare && (word.starts_line ||
                      (columns(line) + 1 + columns(word.text) > doc_width && !opens_block(word.text)))) {
            write_line(out, line);
            line = std::format("{}///", indent);
            bare = true;
        }
        line += ' ';
        if (word.starts_line && !word.marker && opens_block(word.text)) {
            line += "&zwnj;";
        }
        line += word.text;
        bare = false;
    }
    write_line(out, line);
}

// Text that says nothing: no pieces, or text of spaces alone.
bool blank(doc_text const& text) {
    return std::ranges::all_of(text, [](doc_run const& run) {
        return run.kind == doc_run::kind_t::text && std::ranges::all_of(run.text, [](char c) { return c == ' '; });
    });
}

}  // namespace

void write_doc(std::ostream& out, std::string_view indent, member_documentation const& doc,
               std::span<std::string_view const> params, bool has_result) {
    std::vector<std::pair<std::string, doc_text const*>> blocks;  // lead, text
    if (!blank(doc.deprecated)) {
        blocks.emplace_back("@deprecated", &doc.deprecated);
    }
    for (std::string_view const name : params) {
        auto const described = std::ranges::find(doc.params, name, &std::pair<std::string, doc_text>::first);
        if (described != doc.params.end() && !blank(described->second)) {
            blocks.emplace_back(std::format("@param {}", name), &described->second);
        }
    }
    if (has_result && !blank(doc.returns)) {
        blocks.emplace_back("@return", &doc.returns);
    }

    bool const summary = !blank(doc.summary);
    if (summary) {
        write_paragraph(out, indent, {}, doc.summary);
    }
    if (summary && !blocks.empty()) {
        std::print(out, "{}///\n", indent);
    }
    for (auto&& [lead, text] : blocks) {
        write_paragraph(out, indent, lead, *text);
    }
}

void write_include(std::ostream& out, std::string_view name, std::string_view sibling) {
    if (name.starts_with('<')) {
        std::print(out, "#include {}\n", name);
    } else if (name.starts_with("../")) {
        std::print(out, "#include <{}>\n", name.substr(3));
    } else {
        std::print(out, "#include \"{}{}\"\n", sibling, name);
    }
}

void write_includes(std::ostream& out, std::set<std::string> const& includes) {
    for (auto&& include : includes) {
        write_include(out, include);
    }
}

void write_umbrella_file(std::filesystem::path const& path, std::vector<std::string> const& files) {
    auto out = open_output(path);

    std::print(out, "{}#pragma once\n\n", banner);
    for (auto&& file : files) {
        write_include(out, file);
    }
}

emitted::emitted(std::filesystem::path dir) : dir_(std::move(dir)) {}

void emitted::add(std::filesystem::path const& path) {
    auto relative = path.lexically_relative(dir_);
    if (relative.empty() || *relative.begin() == "..") {
        throw std::logic_error(std::format("{} is recorded as generated, and it is not under {}",
                                           wxl::core::unicode::to_utf8(path).chars(),
                                           wxl::core::unicode::to_utf8(dir_).chars()));
    }
    files_.push_back(std::move(relative));
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
