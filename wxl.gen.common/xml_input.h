#pragma once

#include <chrono>
#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

// The generator's XML side. The reader is wxl::xml, imported by xml_input.cpp
// alone; the writers and the editor see only the plain declarations below.
//
// Nothing here interprets what it reads, but for the text of a description: a
// caller gets the document's own strings and decides what they mean, while the
// markup inside a description is read once, here, for both the generator and
// the editor that show it.

// One keyed resource of a XAML dictionary, as declared. The reader hands out
// everything it finds and interprets none of it: what a writer makes of a
// Thickness, a Storyboard or a converter is that writer's business, and the
// walk over a 3 MB document happens once instead of once per interest.
struct dictionary_resource {
    std::string key;
    std::string type;         // the element's local name: SolidColorBrush, Style, Double...
    std::string target_type;  // styles: the TargetType, still carrying its prefix, if any
    std::string alias_for;    // StaticResource aliases: the ResourceKey they forward to
    std::string theme;        // the enclosing theme dictionary's key (Default, Light,
                              // HighContrast); empty outside ThemeDictionaries
    bool scoped = false;      // declared inside some element's own Resources (a template's
                              // local override) -- unreachable from Application.Resources
};

// Every resource in the dictionary that carries a key, in document order.
std::vector<dictionary_resource> dictionary_resources(std::filesystem::path const& dictionary);

// A piece of a description as its markup shows it: a run of text, or an element
// of the HTML and Markdown the text carries, holding pieces of its own -- the
// text of a bold run, the rows of a table. Text is as written, its HTML
// character references turned into the characters they stand for and its runs
// of white space folded into single spaces.
struct doc_run {
    enum class kind_t : std::uint8_t {
        text,         // `text`
        code,         // `...`: `text` the code span as written, its backticks too
        bold,         // **...** or __...__: `text` the mark, holding `runs`
        italic,       // *...* or _..._: `text` the mark, holding `runs`
        superscript,  // <sup>, holding `runs`
        subscript,    // <sub>, holding `runs`
        line_break,   // <br/>
        image,        // <img>: `url` absolute, `text` the alt text
        definitions,  // <termdeflist>: a `definition` in `runs` for each term
        definition,   // <termdef>: `text` the term, `runs` what it stands for
        table,        // <table>: a `row` in `runs` for each row
        row,          // <tr>: a `header_cell` or a `cell` in `runs` for each cell
        header_cell,  // <th>, holding `runs`
        cell,         // <td>, holding `runs`
    };

    kind_t kind = kind_t::text;
    std::string text;
    std::string url;
    std::vector<doc_run> runs;

    bool operator==(doc_run const&) const = default;
};

// A description: its pieces in order, none when it says nothing. A table and a
// list of definitions stand only here, never inside another piece.
using doc_text = std::vector<doc_run>;

// The characters the generator writes a backslash before in a comment, since
// Doxygen would read them as a command, a tag, an entity or a link -- outside a
// code span, whose text Doxygen escapes itself. The reader finds bold and italic
// by the rules of Doxygen's Markdown over the text so written.
inline constexpr std::string_view doxygen_escaped = "\\@#<>&$%\"";

// One member of a .NET documentation file, each part of it read into the
// pieces of its markup.
struct member_documentation {
    doc_text summary;
    std::vector<std::pair<std::string, doc_text>> params;  // name, text
    doc_text returns;
    doc_text deprecated;
};

// The roots the address of an image in a description is relative to: the
// pages of the Windows App SDK's reference, for the .xml beside one of its
// .winmd files, and those of the Windows SDK's reference.
inline constexpr std::string_view app_sdk_image_root = "https://learn.microsoft.com/windows/windows-app-sdk/api/winrt/";
inline constexpr std::string_view windows_sdk_image_root = "https://learn.microsoft.com/uwp/api/";

// A .NET documentation file -- the .xml a .winmd ships beside it, or the
// Windows SDK's reference for a contract -- read once and kept. Opening it only
// finds where each <member> stands and what it is called; a member is parsed
// when it is first asked for, or by parse_some(), which takes the rest a slice
// at a time.
class documentation_file {
public:
    // `image_root` is the root the images of its descriptions are relative to:
    // one of the two above, by where the file comes from.
    //
    // Throws std::runtime_error naming the file and the offset of the member
    // when a member is truncated or has attributes but no name, and naming the
    // file when it holds more than 4 GiB less a byte, which the compact index of
    // its members, of 32-bit offsets, cannot address.
    documentation_file(std::filesystem::path const& file, std::string image_root);
    documentation_file(documentation_file&&) noexcept;
    documentation_file& operator=(documentation_file&&) noexcept;
    ~documentation_file();

    // The member by its documentation ID ("T:Ns.Type",
    // "M:Ns.Type.Method(System.String)"), or null when the file has no such
    // member. Read off the member's element the first time it is asked for
    // and kept by the file, so good for as long as the file is.
    //
    // A member that does not parse throws std::runtime_error naming the file
    // and the member, and so does one with an element the reader would lose,
    // naming it and its line: any but <summary>, <returns>, <param>,
    // <deprecated> and <typeparam> (known, and not shown) at the top, and any
    // at all inside those, such as <see cref="..."/>. Markup inside the text
    // that does not parse is text as written, but for known tags left open
    // with nothing after them where the text ends, which go.
    member_documentation const* find(std::string_view id);

    // Parses members not parsed yet until the deadline passes, passing over
    // those find() has parsed already; whether any are left. Throws as find()
    // does at the first member that does not parse.
    bool parse_some(std::chrono::steady_clock::time_point deadline);

    std::size_t size() const noexcept;

private:
    struct state;
    std::unique_ptr<state> state_;
};
