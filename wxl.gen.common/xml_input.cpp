#include "xml_input.h"

#include <algorithm>
#include <format>
#include <limits>
#include <optional>
#include <span>
#include <stdexcept>

import wxl.core;
import wxl.xml;

std::vector<dictionary_resource> dictionary_resources(std::filesystem::path const& dictionary) {
    std::vector<dictionary_resource> found;

    // Markup, not prose: an element here holds children or one piece of text,
    // never both, so the joining mode saves a node per run of text.
    wxl::xml::document document{wxl::xml::options{.keep_text = false}};

    auto const text = [](wxl::xml::node const& node, std::string_view name) {
        auto const value = node.attribute(name);
        return value ? std::string{value->chars()} : std::string{};
    };

    // The walk is an explicit stack rather than a function calling itself: a
    // resource dictionary nests as deep as the control templates in it, and
    // the reader is careful to make its own traversal iterative for exactly
    // that reason. The theme travels with the frame: it is a property of
    // where the walk stands, and a stack has no other memory of the path.
    struct frame {
        wxl::xml::node const* node;
        std::string theme;
        bool scoped;
    };
    std::vector<frame> pending{{&document.load_file(dictionary), {}, false}};
    while (!pending.empty()) {
        auto const [node, theme, scoped] = std::move(pending.back());
        pending.pop_back();

        // A key is what makes an element a resource; anything keyless is a
        // value inside somebody's setter. Local names throughout: the reader
        // resolves namespaces, so the prefix a dictionary happens to bind --
        // `x:Double`, `media:SolidColorBrush` -- is its business, and the
        // spellings merge on the name that matters.
        if (auto const key = node->attribute("Key")) {
            found.push_back({std::string{key->chars()}, std::string{node->name().chars()},
                             text(*node, "TargetType"), text(*node, "ResourceKey"), theme,
                             scoped});
        }

        // <ResourceDictionary.ThemeDictionaries> is the property element the
        // themes hang under, and each child dictionary's own key -- Default,
        // Light, HighContrast -- names the theme for everything inside it.
        //
        // Anything below an element that is not dictionary machinery -- a
        // control in a template hanging its own <Button.Resources>, a style
        // inside a presenter -- is a local override: the same key, but only
        // for that subtree, and invisible to an application-level lookup.
        std::string_view const name{node->name().chars()};
        bool const themes = name.ends_with(".ThemeDictionaries");
        bool const machinery = name == "ResourceDictionary" || themes ||
                               name.ends_with(".MergedDictionaries");
        for (auto&& child : node->children()) {
            pending.push_back({&child, themes ? text(child, "Key") : theme, scoped || !machinery});
        }
    }
    return found;
}

namespace {

bool is_space(char c) noexcept {
    return c == ' ' || c == '\n' || c == '\r' || c == '\t';
}

// A character Doxygen's Markdown takes for one of a word: an ASCII letter or
// digit, or any byte of a character beyond ASCII. The underscore is not one.
bool doxygen_letter(char c) noexcept {
    auto const byte = static_cast<unsigned char>(c);
    return byte >= 0x80 || (byte >= '0' && byte <= '9') || (byte >= 'a' && byte <= 'z') || (byte >= 'A' && byte <= 'Z');
}

// Text with its runs of white space folded into single spaces, and none at its
// edges.
std::string folded(std::string_view text) {
    std::string out;
    bool space = false;
    for (char const c : text) {
        if (is_space(c)) {
            space = !out.empty();
            continue;
        }
        if (space) {
            out += ' ';
            space = false;
        }
        out += c;
    }
    return out;
}

// The named character references of HTML the descriptions hold, in the
// localized files of the Windows SDK's reference -- &amp; and &nbsp; in the
// text, &quot; in the alt text of an image -- and what each stands for. Any
// other name is text as written.
struct named_reference {
    std::string_view name;
    std::string_view character;
};
constexpr named_reference named_references[] = {{"amp", "&"}, {"nbsp", "\xC2\xA0"}, {"quot", "\""}};

// The character reference at `at` -- one of the names above, &#N; or &#xN; --
// appended to `into` as the character it stands for: the position past it, or
// `at` when none stands there. A number stands for a character only when it is
// a scalar value, as for the XML reader's own references, and no control
// character -- C0 or DEL, but for the tab and the line ends the reading folds
// as white space -- which the comment of a wrapper would carry raw.
std::size_t append_reference(std::string_view const text, std::size_t const at, std::string& into) {
    std::size_t p = at + 1;
    if (p < text.size() && text[p] == '#') {
        bool const hex = p + 1 < text.size() && (text[p + 1] == 'x' || text[p + 1] == 'X');
        char32_t const base = hex ? 16 : 10;
        p += hex ? 2 : 1;
        std::size_t const digits = p;
        char32_t code_point = 0;
        for (; p < text.size(); ++p) {
            char const c = text[p];
            int digit = -1;
            if (c >= '0' && c <= '9') {
                digit = c - '0';
            } else if (hex && c >= 'a' && c <= 'f') {
                digit = c - 'a' + 10;
            } else if (hex && c >= 'A' && c <= 'F') {
                digit = c - 'A' + 10;
            }
            if (digit < 0) {
                break;
            }
            // Once past the last code point the number grows no further, so it cannot overflow.
            if (code_point <= wxl::core::unicode::max_code_point) {
                code_point = static_cast<char32_t>(code_point * base + static_cast<char32_t>(digit));
            }
        }
        bool const control = (code_point < 0x20 && !is_space(static_cast<char>(code_point))) || code_point == 0x7F;
        if (p == digits || p == text.size() || text[p] != ';' || !wxl::core::unicode::is_scalar_value(code_point) ||
            control) {
            return at;
        }
        wxl::core::unicode::append_utf8(into, code_point);
        return p + 1;
    }
    std::size_t const name = p;
    while (p < text.size() && ((text[p] >= 'a' && text[p] <= 'z') || (text[p] >= 'A' && text[p] <= 'Z') ||
                               (text[p] >= '0' && text[p] <= '9'))) {
        ++p;
    }
    if (p == text.size() || text[p] != ';') {
        return at;
    }
    auto const known = std::ranges::find(named_references, text.substr(name, p - name), &named_reference::name);
    if (known == std::end(named_references)) {
        return at;
    }
    into += known->character;
    return p + 1;
}

// A description's text with its character references decoded, and the position
// of every byte that came of one, in order: such a byte is text to the markup
// of the description, never part of a tag.
struct decoded_text {
    std::string text;
    std::vector<std::size_t> references;
};

// The text of a part as XML decoding leaves it, with the HTML character
// references in it turned into the characters they stand for, once: what one
// stands for is not read as a reference again.
decoded_text html_decoded(std::string text) {
    if (!text.contains('&')) {
        return {std::move(text), {}};
    }
    decoded_text decoded;
    decoded.text.reserve(text.size());
    for (std::size_t at = 0; at < text.size();) {
        std::size_t const written = decoded.text.size();
        std::size_t const past = text[at] == '&' ? append_reference(text, at, decoded.text) : at;
        if (past == at) {
            decoded.text += text[at++];
            continue;
        }
        for (std::size_t byte = written; byte < decoded.text.size(); ++byte) {
            decoded.references.push_back(byte);
        }
        at = past;
    }
    return decoded;
}

// The tags the markup of a description is made of. Any other name in angle
// brackets is text -- IVector<String>, <xxx:yyy> -- and so is <name> outside a
// <termdef>, whose first child it is and where the list looks for it.
constexpr std::string_view markup_names[] = {"br", "img", "sup", "sub", "table", "tr", "th", "td", "termdeflist",
                                             "termdef"};

// Whether an element of that name holds content, as all but a line break and
// an image do.
bool holds_content(std::string_view name) noexcept {
    return name != "br" && name != "img";
}

// A tag of the markup and where it stands in the text.
struct markup_tag {
    std::string_view name;
    std::size_t begin = 0;  // its '<'
    std::size_t end = 0;    // past its '>'
    bool closing = false;
    bool self_closing = false;
    std::string_view src;  // an image's two attributes
    std::string_view alt;
};

// The markup of one description read into its pieces: one flow of text, white
// space folded as it goes, and one in each cell and definition. Markup that does
// not parse is text as written, but for known tags left open with nothing after
// them where the text ends, whose content the file lost. Bold and italic follow
// the rules of Doxygen's Markdown, so that the editor shows what the site does.
// The text is read with its character references decoded: what one stands for
// is no part of a tag, and to Markdown it is what the writer makes of it, a
// character like any other.
class markup_reader {
public:
    markup_reader(decoded_text const& text, std::string_view image_root) noexcept
        : text_(text.text), references_(text.references), image_root_(image_root) {}

    doc_text read() {
        doc_text pieces;
        flow(0, text_.size(), pieces, true);
        return pieces;
    }

private:
    // The text from `at` to `end`, into `into`. `blocks` where a table or a list
    // may stand: at the top of the description, and nowhere inside another piece.
    void flow(std::size_t at, std::size_t const end, doc_text& into, bool const blocks) {
        while (at < end) {
            if (auto const tag = tag_at(at, end)) {
                if (std::size_t const after = element(*tag, end, into, blocks); after != at) {
                    at = after;
                    continue;
                }
            }
            if (at >= quiet_) {
                // A code span is a piece of its own, as written, which the writer
                // writes so; backticks passed over are text.
                if (std::size_t const after = code_span(at, end); after != at) {
                    std::string_view const span = text_.substr(at, after - at);
                    if (span.find_first_not_of('`') == std::string_view::npos) {
                        for (; at < after; ++at) {
                            character(into, text_[at]);
                        }
                    } else {
                        settle(into);
                        into.push_back({.kind = doc_run::kind_t::code, .text = folded(span)});
                        at = after;
                    }
                    continue;
                }
                if (text_[at] == '"') {
                    quiet_ = quoted(at, end);
                } else if (std::size_t const after = emphasis(at, end, into); after != at) {
                    at = after;
                    continue;
                }
            }
            if (std::size_t const after = remnant(at, end); after != at) {
                at = after;
                continue;
            }
            character(into, text_[at]);
            ++at;
        }
    }

    // One character of text, white space folded.
    void character(doc_text& into, char const c) {
        if (is_space(c)) {
            space_ = written_;
            return;
        }
        settle(into);
        text_run(into).text += c;
    }

    // The text run that ends `into`: a new one, when something else ends it.
    static doc_run& text_run(doc_text& into) {
        if (into.empty() || into.back().kind != doc_run::kind_t::text) {
            into.emplace_back();
        }
        return into.back();
    }

    // Before the next character or element: the space due, as text in front of it.
    void settle(doc_text& into) {
        if (space_) {
            text_run(into).text += ' ';
            space_ = false;
        }
        written_ = true;
    }

    void restart() noexcept {
        space_ = false;
        written_ = false;
    }

    // The element `tag` opens, read into `into`; the position past it, or the
    // tag's own when it opens none and is text.
    std::size_t element(markup_tag const& tag, std::size_t const end, doc_text& into, bool const blocks) {
        if (!tag.closing && !tag.self_closing && holds_content(tag.name) && only_open_tags(tag.end)) {
            return end;
        }
        if (tag.closing) {
            return tag.begin;
        }
        if (tag.name == "br") {
            restart();
            into.push_back({.kind = doc_run::kind_t::line_break});
            return tag.end;
        }
        if (tag.name == "img") {
            auto url = image_url(tag);
            if (!url) {
                return tag.begin;
            }
            settle(into);
            into.push_back({.kind = doc_run::kind_t::image, .text = std::string {tag.alt}, .url = std::move(*url)});
            return tag.end;
        }
        bool const script = tag.name == "sup" || tag.name == "sub";
        bool const block = tag.name == "table" || tag.name == "termdeflist";
        auto const close = script || (block && blocks) ? closing_tag(tag, end) : std::nullopt;
        if (!close) {
            return tag.begin;
        }
        std::size_t const content = content_;
        content_ = std::string_view::npos;
        if (script) {
            settle(into);
            auto const kind = tag.name == "sup" ? doc_run::kind_t::superscript : doc_run::kind_t::subscript;
            flow(tag.end, close->begin, into.emplace_back(doc_run {.kind = kind}).runs, false);
            content_ = content;
            return close->end;
        }
        bool const space = space_;
        bool const written = written_;
        std::size_t const quiet = quiet_;
        restart();
        doc_run held {.kind = tag.name == "table" ? doc_run::kind_t::table : doc_run::kind_t::definitions};
        bool const read = tag.name == "table" ? table(tag.end, close->begin, held.runs)
                                              : definitions(tag.end, close->begin, held.runs);
        content_ = content;
        if (!read) {
            space_ = space;
            written_ = written;
            quiet_ = quiet;
            return tag.begin;
        }
        into.push_back(std::move(held));
        restart();
        return close->end;
    }

    // Whether nothing but white space and opening tags of elements that hold
    // content follows `at` to the end of the text.
    bool only_open_tags(std::size_t at) const {
        for (;;) {
            at = skip_space(at, text_.size());
            if (at == text_.size()) {
                return true;
            }
            auto const tag = tag_at(at, text_.size());
            if (!tag || tag->closing || tag->self_closing || !holds_content(tag->name)) {
                return false;
            }
            at = tag->end;
        }
    }

    // The rows of a table and the cells of each, with white space between them and
    // nothing else, since text there would belong to no cell; whether it is one.
    bool table(std::size_t at, std::size_t const end, doc_text& rows) {
        for (at = skip_space(at, end); at < end; at = skip_space(at, end)) {
            auto const row = child_tag(at, end, "tr", "tr");
            auto const row_close = row ? closing_tag(*row, end) : std::nullopt;
            if (!row_close) {
                return false;
            }
            doc_text& cells = rows.emplace_back(doc_run {.kind = doc_run::kind_t::row}).runs;
            for (std::size_t cell_at = skip_space(row->end, row_close->begin); cell_at < row_close->begin;
                 cell_at = skip_space(cell_at, row_close->begin)) {
                auto const cell = child_tag(cell_at, row_close->begin, "td", "th");
                auto const cell_close = cell ? closing_tag(*cell, row_close->begin) : std::nullopt;
                if (!cell_close) {
                    return false;
                }
                auto const kind = cell->name == "th" ? doc_run::kind_t::header_cell : doc_run::kind_t::cell;
                restart();
                flow(cell->end, cell_close->begin, cells.emplace_back(doc_run {.kind = kind}).runs, false);
                cell_at = cell_close->end;
            }
            at = row_close->end;
        }
        return true;
    }

    // The terms of a list, each with what it stands for: a <termdef> starts with
    // its term in <name>, and the rest of it is a flow of text. Whether it is one.
    bool definitions(std::size_t at, std::size_t const end, doc_text& items) {
        constexpr std::string_view term_open = "<name>";
        constexpr std::string_view term_close = "</name>";
        for (at = skip_space(at, end); at < end; at = skip_space(at, end)) {
            auto const item = child_tag(at, end, "termdef", "termdef");
            auto const item_close = item ? closing_tag(*item, end) : std::nullopt;
            if (!item_close) {
                return false;
            }
            std::size_t const term = skip_space(item->end, item_close->begin);
            std::size_t const term_end = find_markup(term_close, term, item_close->begin);
            std::string name;
            if (text_.substr(term).starts_with(term_open) && !referenced(term, term + term_open.size()) &&
                term_end != std::string_view::npos) {
                name = folded(text_.substr(term + term_open.size(), term_end - term - term_open.size()));
            }
            if (name.empty()) {
                return false;
            }
            restart();
            doc_run& definition =
                items.emplace_back(doc_run {.kind = doc_run::kind_t::definition, .text = std::move(name)});
            flow(term_end + term_close.size(), item_close->begin, definition.runs, false);
            at = item_close->end;
        }
        return true;
    }

    // A code span opening at `at`, which Doxygen's Markdown leaves as it is: the
    // position past it, or `at` when there is none. As many backticks close it;
    // inside one of a single backtick a doubled one is passed over; three or
    // more backticks never closed are passed over alone, and the position is
    // past them. A quote ends none: Doxygen ends a span at one only with
    // MARKDOWN_STRICT off, and the site keeps it on.
    std::size_t code_span(std::size_t const at, std::size_t const end) const {
        if (text_[at] != '`') {
            return at;
        }
        std::size_t ticks = 0;
        while (at + ticks < end && text_[at + ticks] == '`') {
            ++ticks;
        }
        std::size_t closing = 0;  // backticks of the run that may close it, met so far
        std::size_t p = at + ticks;
        for (; p < end; ++p) {
            char const c = text_[p];
            if (c == '`') {
                ++closing;
                bool const doubled = p + 1 < end && text_[p + 1] == '`';
                if (ticks == 1 && doubled) {
                    ++p;
                    closing = 0;
                } else if (ticks == 1 || (closing == ticks && !doubled)) {
                    break;
                } else if (closing == ticks) {
                    --closing;
                }
            } else {
                closing = 0;
            }
        }
        if (p == end) {
            return ticks >= 3 ? at + ticks : at;
        }
        while (p < end && text_[p] == '`') {
            ++p;
        }
        return p;
    }

    // A quoted string from the '"' at `at`, which Doxygen's Markdown leaves as it
    // is but for the tags in it: the position past its closing quote, or `at`
    // when there is none; a block means another line, and none. An image ends
    // it, as the first quote of its attributes does on the site. Left out are
    // two things the text here does not tell: Doxygen takes no string with two
    // line ends in it, and the writer's wrapping puts them; and it pairs the
    // quotes past that first one anew, the image's own among them.
    std::size_t quoted(std::size_t const at, std::size_t const end) const {
        for (std::size_t q = at + 1; q < end; ++q) {
            if (text_[q] == '"') {
                return q + 1;
            }
            if (auto const tag = tag_at(q, end)) {
                if (tag->name == "table" || tag->name == "termdeflist") {
                    return at;
                }
                if (tag->name == "img" && !tag->closing && image_url(*tag)) {
                    return q;
                }
                if (tag->name != "img") {
                    q = tag->end - 1;
                }
            }
        }
        return at;
    }

    // Bold or italic opening at `at`, read into `into` up to where it closes; the
    // position past it, or `at` when none opens there. One mark is italic, two
    // bold, three both.
    std::size_t emphasis(std::size_t const at, std::size_t const end, doc_text& into) {
        char const mark = text_[at];
        if ((mark != '*' && mark != '_') || !may_open(at)) {
            return at;
        }
        std::size_t width = 1;
        while (at + width < end && text_[at + width] == mark) {
            ++width;
        }
        std::size_t const inside = at + width;
        if (width > 3 || inside >= end || is_space(text_[inside]) || (width < 3 && !may_follow(mark, text_[inside]))) {
            return at;
        }
        std::size_t const close = emphasis_end(mark, width, inside, end);
        if (close == std::string_view::npos) {
            return at;
        }
        // The text inside is read before the space due in front is settled: a
        // command in a code span there keeps the emphasis from closing after all,
        // and the text is then read again as text.
        bool const space = space_;
        bool const written = written_;
        std::size_t const quiet = quiet_;
        std::size_t const outer = content_;
        space_ = false;
        written_ = true;
        content_ = inside;
        doc_text inner;
        flow(inside, close, inner, false);
        content_ = outer;
        if (holds_command(inner)) {
            space_ = space;
            written_ = written;
            quiet_ = quiet;
            return at;
        }
        if (space) {
            text_run(into).text += ' ';
        }
        doc_text* runs = &into;
        if (width != 2) {
            runs = &runs->emplace_back(doc_run {.kind = doc_run::kind_t::italic, .text = std::string(1, mark)}).runs;
        }
        if (width != 1) {
            runs = &runs->emplace_back(doc_run {.kind = doc_run::kind_t::bold, .text = std::string(2, mark)}).runs;
        }
        *runs = std::move(inner);
        // The closing mark is written, and white space after it stays even after
        // a line break: Doxygen closes no emphasis with a letter or digit after it.
        written_ = true;
        return close + width;
    }

    // Whether a code span in `text`, at any depth, holds what Doxygen's search for
    // the mark that closes emphasis takes for a command and ends at, the emphasis
    // with it: a \ or an @ before a letter or a digit, with none of ( { [ < \ @
    // before it. The writer leaves a span as it is and escapes every \ and @
    // elsewhere. A block command such as \code the search passes over to its end
    // command and ends there, which comes to the same here as long as one follows.
    static bool holds_command(doc_text const& text) {
        return std::ranges::any_of(text, [](doc_run const& run) {
            if (run.kind == doc_run::kind_t::code) {
                for (std::size_t at = 1; at + 1 < run.text.size(); ++at) {
                    char const c = run.text[at];
                    if ((c == '\\' || c == '@') && doxygen_letter(run.text[at + 1]) &&
                        !std::string_view {"({[<\\@"}.contains(run.text[at - 1])) {
                        return true;
                    }
                }
            }
            return holds_command(run.runs);
        });
    }

    // Whether Doxygen lets emphasis open at `at` by what stands before it:
    // nothing, white space, one of ' < > { ( [ , : ; or a no-break space, or a /
    // before two *, which Doxygen parts with an entity lest /** open a comment --
    // or, inside other emphasis, the first character or the one after it,
    // before which Doxygen does not look.
    bool may_open(std::size_t const at) const {
        if (at == 0 || at == content_) {
            return true;
        }
        if (content_ != std::string_view::npos && at == content_ + 1 && !doxygen_escaped.contains(text_[content_])) {
            return true;
        }
        char const before = text_[at - 1];
        return is_space(before) || std::string_view {"'<>{([,:;"}.contains(before) ||
               (before == '/' && text_.substr(at, 2) == "**") ||
               (at >= 2 && text_[at - 2] == '\xC2' && before == '\xA0');
    }

    // Whether Doxygen lets emphasis open with `mark` before `c`: a letter or a
    // digit, one of - + ! ? $ @ & * _ % [ ( . > : , ; ' " ` \ -- and a character
    // written with a backslash; the < of a line break or an image, which the
    // writer starts with an entity there; or a / after a *, where Doxygen puts
    // one itself, lest */ end a comment. Any other element has an end tag,
    // which ends emphasis here as on the site.
    static bool may_follow(char const mark, char const c) noexcept {
        return doxygen_letter(c) || doxygen_escaped.contains(c) || (mark == '*' && c == '/') ||
               std::string_view {"-+!?$@&*_%[(.>:,;'\"`\\"}.contains(c);
    }

    // Where emphasis of `width` marks that holds the text from `inside` closes,
    // by Doxygen's search over the text as written: at the first run of as many
    // marks after a character other than white space -- white space the flow
    // drops after a line break is none -- with no letter or digit after it, and
    // none of ( { [ < \ @ before it. An end tag on the way ends the search, and
    // the emphasis with it. Doxygen's search stops at a command too, which only a
    // code span can hold, and emphasis() looks for that in what it reads inside.
    std::size_t emphasis_end(char const mark, std::size_t const width, std::size_t const inside,
                             std::size_t const end) const {
        for (std::size_t at = inside; at < end;) {
            char const c = text_[at];
            if (c == '<' && at + 1 < end && text_[at + 1] == '/') {
                return std::string_view::npos;
            }
            if (c != mark || at == inside || std::string_view {"({[<\\@"}.contains(text_[at - 1])) {
                ++at;
                continue;
            }
            std::size_t run = 1;
            while (at + run < end && text_[at + run] == mark) {
                ++run;
            }
            if (run != width || (at + run < text_.size() && doxygen_letter(text_[at + run]))) {
                at += run;
                continue;
            }
            if (!is_space(text_[at - 1]) || after_line_break(at)) {
                return at;
            }
            at += width == 2 ? 2 : 1;
        }
        return std::string_view::npos;
    }

    // Whether the white space before `at` follows a line break, where the flow
    // drops it.
    bool after_line_break(std::size_t at) const {
        while (at > 0 && is_space(text_[at - 1])) {
            --at;
        }
        std::size_t const open = at > 0 ? text_.rfind('<', at - 1) : std::string_view::npos;
        auto const tag = open == std::string_view::npos ? std::nullopt : tag_at(open, at);
        return tag && tag->name == "br" && !tag->closing && tag->end == at;
    }

    // The remnant of a Markdown image that lost its address: "!caption", a line
    // break -- <br/>, or two spaces ending the line -- and, after it and any
    // blank lines, the caption again alone on its line. It goes whole: the
    // position past it, or `at` when the text there is not one. Its "!", its
    // break and its line ends are the file's own: a character a reference stands
    // for there makes it text.
    std::size_t remnant(std::size_t const at, std::size_t const end) const {
        if (text_[at] != '!' || (at > 0 && !is_space(text_[at - 1])) || at + 1 >= end || is_space(text_[at + 1])) {
            return at;
        }
        std::size_t const line_end = std::min(text_.find('\n', at), end);
        std::size_t caption_end = line_end;
        std::size_t after = line_end;
        for (std::size_t p = at + 1; p < line_end; ++p) {
            if (auto const tag = tag_at(p, end)) {
                if (tag->closing || tag->name != "br") {
                    return at;
                }
                caption_end = p;
                after = tag->end;
                break;
            }
        }
        if (caption_end == line_end) {
            std::size_t const spaces_end = line_end > at && text_[line_end - 1] == '\r' ? line_end - 1 : line_end;
            std::size_t spaces = spaces_end;
            while (spaces > at + 1 && text_[spaces - 1] == ' ') {
                --spaces;
            }
            if (line_end == end || spaces_end - spaces < 2) {
                return at;
            }
            caption_end = spaces;
            after = line_end + 1;
        }
        std::string_view caption = text_.substr(at + 1, caption_end - at - 1);
        while (!caption.empty() && is_space(caption.back())) {
            caption.remove_suffix(1);
        }
        std::size_t const repeated = skip_space(after, end);
        std::size_t const past = repeated + caption.size();
        if (caption.empty() || past > end || text_.substr(repeated, caption.size()) != caption) {
            return at;
        }
        std::size_t line_rest = past;
        while (line_rest < end && (text_[line_rest] == ' ' || text_[line_rest] == '\t')) {
            ++line_rest;
        }
        bool const alone = line_rest == end || text_[line_rest] == '\r' || text_[line_rest] == '\n';
        bool const marked = !referenced(at, at + 1) && !referenced(at + 1 + caption.size(), repeated) &&
                            !referenced(past, line_rest + 1);
        return alone && marked ? past : at;
    }

    // The tag at `at`, when one of the markup starts there and ends before
    // `limit`: past its name, attributes with quoted values and then '>' or "/>".
    // One that does not end so is text, and so is one with an attribute the form
    // does not carry -- any but an image's source and alt text, and its align,
    // which goes -- and one with a character a reference stands for anywhere but
    // in a value.
    std::optional<markup_tag> tag_at(std::size_t const at, std::size_t const limit) const {
        if (text_[at] != '<') {
            return std::nullopt;
        }
        markup_tag tag {.begin = at};
        std::size_t p = at + 1;
        tag.closing = p < limit && text_[p] == '/';
        if (tag.closing) {
            ++p;
        }
        std::size_t const name = p;
        while (p < limit && text_[p] >= 'a' && text_[p] <= 'z') {
            ++p;
        }
        tag.name = text_.substr(name, p - name);
        if (p == limit || (text_[p] != '>' && text_[p] != '/' && !is_space(text_[p])) ||
            std::ranges::find(markup_names, tag.name) == std::end(markup_names)) {
            return std::nullopt;
        }
        std::size_t syntax = at;  // where the tag's own characters go on, past a value
        for (;;) {
            p = skip_space(p, limit);
            if (p == limit) {
                return std::nullopt;
            }
            if (text_[p] == '>' || (text_.substr(p, 2) == "/>" && p + 2 <= limit)) {
                tag.self_closing = text_[p] == '/';
                tag.end = p + (tag.self_closing ? 2 : 1);
                if (referenced(syntax, tag.end)) {
                    return std::nullopt;
                }
                return tag;
            }
            std::size_t const attribute = p;
            while (p < limit && text_[p] != '=' && text_[p] != '>' && text_[p] != '/' && !is_space(text_[p])) {
                ++p;
            }
            std::string_view const attribute_name = text_.substr(attribute, p - attribute);
            p = skip_space(p, limit);
            if (attribute_name.empty() || p == limit || text_[p] != '=') {
                return std::nullopt;
            }
            p = skip_space(p + 1, limit);
            if (p == limit || (text_[p] != '"' && text_[p] != '\'')) {
                return std::nullopt;
            }
            std::size_t const value_end = find_markup(text_.substr(p, 1), p + 1, limit);
            if (value_end == std::string_view::npos || referenced(syntax, p + 1)) {
                return std::nullopt;
            }
            std::string_view const value = text_.substr(p + 1, value_end - p - 1);
            if (tag.name == "img" && attribute_name == "src") {
                tag.src = value;
            } else if (tag.name == "img" && attribute_name == "alt") {
                tag.alt = value;
            } else if (tag.name != "img" || attribute_name != "align") {
                return std::nullopt;
            }
            syntax = value_end;
            p = value_end + 1;
        }
    }

    // The tag that closes the element `open` opens, before `end`, or none;
    // elements of the same name inside it nest.
    std::optional<markup_tag> closing_tag(markup_tag const& open, std::size_t const end) const {
        if (open.self_closing) {
            return std::nullopt;
        }
        std::size_t depth = 0;
        for (std::size_t at = open.end; at < end;) {
            auto const tag = tag_at(at, end);
            if (!tag) {
                ++at;
                continue;
            }
            if (tag->name == open.name && !tag->self_closing) {
                if (!tag->closing) {
                    ++depth;
                } else if (depth == 0) {
                    return tag;
                } else {
                    --depth;
                }
            }
            at = tag->end;
        }
        return std::nullopt;
    }

    // The opening tag at `at` of `name` or `other`, or none: the children a parent
    // holds besides white space.
    std::optional<markup_tag> child_tag(std::size_t const at, std::size_t const end, std::string_view const name,
                                        std::string_view const other) const {
        auto const tag = tag_at(at, end);
        if (!tag || tag->closing || (tag->name != name && tag->name != other)) {
            return std::nullopt;
        }
        return tag;
    }

    // An image's address as Microsoft's pages give it: the path relative to the
    // page, less its "./", under the root of the reference the description comes
    // from; an absolute one as it is. None for any other.
    std::optional<std::string> image_url(markup_tag const& tag) const {
        if (tag.src.starts_with("./")) {
            return std::format("{}{}", image_root_, tag.src.substr(2));
        }
        if (tag.src.starts_with("https://") || tag.src.starts_with("http://")) {
            return std::string {tag.src};
        }
        return std::nullopt;
    }

    std::size_t skip_space(std::size_t at, std::size_t const end) const noexcept {
        while (at < end && is_space(text_[at])) {
            ++at;
        }
        return at;
    }

    // Whether a byte from `begin` to `end` came of a character reference.
    bool referenced(std::size_t const begin, std::size_t const end) const noexcept {
        auto const found = std::ranges::lower_bound(references_, begin);
        return found != references_.end() && *found < end;
    }

    // Where `what` stands in the text from `from` to `end` made of the file's own
    // characters, none of them one a reference stands for; npos where nowhere.
    std::size_t find_markup(std::string_view const what, std::size_t from, std::size_t const end) const noexcept {
        for (;; ++from) {
            from = text_.substr(0, end).find(what, from);
            if (from == std::string_view::npos || !referenced(from, from + what.size())) {
                return from;
            }
        }
    }

    std::string_view const text_;
    std::span<std::size_t const> const references_;  // decoded_text::references
    std::string_view const image_root_;
    bool space_ = false;    // a space is due before the next character or element
    bool written_ = false;  // the flow holds something a space may follow
    std::size_t quiet_ = 0;  // up to here the text is a quoted string: no emphasis, no code span
    std::size_t content_ = std::string_view::npos;  // where the emphasis the flow is in begins
};

// The file as an error names it: through to_utf8, not path::string(), which
// converts through the ANSI code page -- it garbles a name the page cannot
// spell, or on Windows throws std::system_error for it, and the error being
// reported is lost.
std::string file_name(std::filesystem::path const& file) {
    return std::string {wxl::core::unicode::to_utf8(file).chars()};
}

// A member list that cannot be read to its end. Taken for a shorter one, a
// truncated file would document fewer members and say nothing.
[[noreturn]] void broken_member(std::filesystem::path const& file, std::size_t at, std::string_view what) {
    throw std::runtime_error(std::format("{}: documentation member at offset {} {}", file_name(file), at, what));
}

}  // namespace

// The file's text, owned and kept, and where each member stands in it. The
// index is an open-addressing table of member numbers: one allocation for the
// whole of it and nothing to sort. Parsed members live in one arena, their
// nodes viewing the text in place.
struct documentation_file::state {
    // The offsets of a member take 32 bits, which keeps the index compact: the
    // largest of the files, the Windows SDK's reference for its universal
    // contract, is some 17 MB. The constructor refuses a text the offsets cannot
    // address before it takes one, so no cast cuts one short; and member numbers,
    // 32 bits too, cannot overflow, as a member is at least as long as
    // <member name=""/>.
    struct member {
        std::string_view id;
        std::uint32_t begin = 0;  // the member's element, from '<' to after its end tag
        std::uint32_t end = 0;
        wxl::xml::node const* node = nullptr;
        bool parsed = false;
    };

    explicit state(std::string root) : image_root(std::move(root)) {}

    std::filesystem::path file;    // named by the error of a member that does not parse
    std::string const image_root;  // what the images of the descriptions are relative to
    std::string text;
    wxl::xml::arena arena;
    std::vector<member> members;       // in document order
    std::vector<std::uint32_t> table;  // a member's number plus one, 0 free; a power of two, at most half full
    std::size_t next = 0;              // the first member parse_some() has not looked at

    // The documentation of the members asked for, by member number: only
    // those pay for it, out of the tens of thousands a file holds.
    std::map<std::uint32_t, member_documentation> documented;

    std::size_t home(std::string_view id) const noexcept {
        return std::hash<std::string_view> {}(id) & (table.size() - 1);
    }

    member* find(std::string_view id) noexcept {
        if (table.empty()) {
            return nullptr;
        }
        for (std::size_t at = home(id); table[at]; at = (at + 1) & (table.size() - 1)) {
            if (members[table[at] - 1].id == id) {
                return &members[table[at] - 1];
            }
        }
        return nullptr;
    }

    // The member's element parsed where it stands: the byte after its end tag
    // becomes the zero the parser stops at, and then is put back. A member
    // that does not parse throws, naming the file and the member, and stays
    // unparsed: asked for again, it throws again.
    void parse(member& m) {
        struct put_back {
            char& at;
            char const kept;
            ~put_back() { at = kept; }
        } const after {text[m.end], text[m.end]};
        after.at = '\0';
        try {
            std::string_view const fragment {text.data() + m.begin, m.end - m.begin};
            wxl::xml::validate_utf8(fragment);
            wxl::xml::parser reader {fragment, arena};
            m.node = &reader.parse();
        } catch (wxl::xml::exception const& error) {
            throw std::runtime_error(std::format("{}: {}: {}", file_name(file), m.id, error.what()));
        }
        m.parsed = true;
    }

    // The line of the file a node of the member stands on, by the line the
    // parser gave it: the member is parsed where it stands, and counted from 1
    // at its start tag.
    int line_of(member const& m, int line) const {
        return line + static_cast<int>(std::count(text.begin(), text.begin() + m.begin, '\n'));
    }

    // A part of the member holds text alone: an element inside it would be lost,
    // and is an error instead.
    void text_alone(member const& m, wxl::xml::node const& part) const {
        for (auto&& child : part.children()) {
            if (child.is_element()) {
                throw std::runtime_error(std::format("{}: {}: line {}: <{}> inside <{}>, which holds text alone",
                                                     file_name(file), m.id, line_of(m, child.line()),
                                                     std::string_view {child.name().chars()},
                                                     std::string_view {part.name().chars()}));
            }
        }
    }

    // A part of the member -- its summary, a parameter -- read into the pieces of
    // its markup.
    doc_text described(member const& m, wxl::xml::node const& part) const {
        text_alone(m, part);
        std::string joined;
        for (auto const piece : part.text_pieces()) {
            joined += piece.chars();
        }
        decoded_text const text = html_decoded(std::move(joined));
        return markup_reader {text, image_root}.read();
    }
};

documentation_file::documentation_file(std::filesystem::path const& file, std::string image_root)
    : state_(std::make_unique<state>(std::move(image_root))) {
    state& st = *state_;
    st.file = file;
    st.text = wxl::xml::read_file(file);
    std::string_view const text = st.text;
    if (text.size() > std::numeric_limits<std::uint32_t>::max()) {
        throw std::runtime_error(std::format("{}: {} bytes, more than the {} the index of a documentation file "
                                             "addresses",
                                             file_name(file), text.size(), std::numeric_limits<std::uint32_t>::max()));
    }

    // A member is found by its start tag and its end tag, and named by its
    // name attribute -- a documentation ID, which never holds a character a
    // document would have to escape.
    constexpr std::string_view open = "<member";
    constexpr std::string_view close = "</member>";
    for (std::size_t at = text.find(open); at != std::string_view::npos; at = text.find(open, at)) {
        std::size_t const tag_end = text.find('>', at);
        if (tag_end == std::string_view::npos) {
            broken_member(file, at, "is truncated: no '>' closes its start tag");
        }
        char const after = text[at + open.size()];
        if (after != ' ' && after != '\t' && after != '\r' && after != '\n') {
            at = tag_end;  // <members>, or a <member> without attributes
            continue;
        }
        std::string_view const tag = text.substr(at, tag_end + 1 - at);
        std::size_t end = tag_end + 1;
        if (!tag.ends_with("/>")) {
            std::size_t const closing = text.find(close, tag_end);
            if (closing == std::string_view::npos) {
                broken_member(file, at, "is truncated: no </member> closes it");
            }
            end = closing + close.size();
        }

        // A member with attributes but no name is as broken as a truncated one:
        // nothing could ever ask for it.
        std::size_t const name = tag.find("name=");
        char const quote = name == std::string_view::npos ? '\0' : tag[name + 5];
        std::size_t const value_end =
            quote == '"' || quote == '\'' ? tag.find(quote, name + 6) : std::string_view::npos;
        if (value_end == std::string_view::npos) {
            broken_member(file, at, "has no name: its start tag holds no quoted name attribute");
        }
        st.members.push_back({tag.substr(name + 6, value_end - name - 6), static_cast<std::uint32_t>(at),
                              static_cast<std::uint32_t>(end)});
        at = end;
    }

    st.table.assign(std::bit_ceil(2 * st.members.size() + 1), 0);
    for (std::uint32_t number = 0; number < st.members.size(); ++number) {
        std::string_view const id = st.members[number].id;
        std::size_t at = st.home(id);
        while (st.table[at] && st.members[st.table[at] - 1].id != id) {
            at = (at + 1) & (st.table.size() - 1);
        }
        st.table[at] = number + 1;  // a later member of the same ID wins
    }
}

documentation_file::documentation_file(documentation_file&&) noexcept = default;
documentation_file& documentation_file::operator=(documentation_file&&) noexcept = default;
documentation_file::~documentation_file() = default;

member_documentation const* documentation_file::find(std::string_view id) {
    state::member* const member = state_->find(id);
    if (!member) {
        return nullptr;
    }
    if (!member->parsed) {
        state_->parse(*member);
    }

    auto const number = static_cast<std::uint32_t>(member - state_->members.data());
    if (auto const kept = state_->documented.find(number); kept != state_->documented.end()) {
        return &kept->second;
    }

    // Kept only once read whole: a member that throws throws again when asked
    // again, rather than passing for one that says less.
    member_documentation documentation;
    for (auto&& part : member->node->children()) {
        if (!part.is_element()) {
            continue;
        }
        std::string_view const kind {part.name().chars()};
        if (kind == "summary") {
            documentation.summary = state_->described(*member, part);
        } else if (kind == "returns") {
            documentation.returns = state_->described(*member, part);
        } else if (kind == "deprecated") {
            documentation.deprecated = state_->described(*member, part);
        } else if (kind == "param") {
            auto const param = part.attribute("name");
            documentation.params.emplace_back(param ? std::string {param->chars()} : std::string {},
                                              state_->described(*member, part));
        } else if (kind == "typeparam") {
            // Known, and not shown: a wxl wrapper has no type parameters.
            state_->text_alone(*member, part);
        } else {
            throw std::runtime_error(std::format("{}: {}: line {}: <{}> is no part of a member's documentation",
                                                 file_name(state_->file), member->id,
                                                 state_->line_of(*member, part.line()), kind));
        }
    }
    return &state_->documented.emplace(number, std::move(documentation)).first->second;
}

bool documentation_file::parse_some(std::chrono::steady_clock::time_point deadline) {
    state& st = *state_;
    while (st.next < st.members.size()) {
        state::member& member = st.members[st.next++];
        // Parsed already by find(), at once, when it was asked for.
        if (member.parsed) {
            continue;
        }
        st.parse(member);
        if (std::chrono::steady_clock::now() >= deadline) {
            break;
        }
    }
    while (st.next < st.members.size() && st.members[st.next].parsed) {
        ++st.next;
    }
    return st.next < st.members.size();
}

std::size_t documentation_file::size() const noexcept {
    return state_->members.size();
}
