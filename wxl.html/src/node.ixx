// The lightweight DOM the parser hands to the builder: one concrete node
// type, children on an intrusive list through a field the node itself
// carries, attributes as a span in the arena -- the storage scheme of
// wxl.xml's tree, taken over deliberately (design.md, "Слои внутри").
//
// Unlike wxl.xml's tree, nothing here is a view into the source: entities
// and whitespace collapsing rewrite almost every piece of text anyway, so
// every view points into the document's own arena and the tree owes the
// input nothing once the parse returns.
//
// Every one of those views is null-terminated at [size], deliberately: the
// terminator costs the arena one wchar_t per string, and it is what lets a
// view cross into COM/WinRT as a string reference (the param::hstring
// contract -- zero right after the view) without a copy at the call.

export module wxl.html:node;

import std;
export import wxl.core;

export namespace wxl::html {

class node;

/// The children of a node: the nodes themselves, linked through the field
/// they carry for it. Nothing is allocated to hold a child.
using node_list = core::intrusive_slist<node>;

/// What a node is. The tag set is closed by design -- an unknown tag never
/// reaches the tree, its content does -- so the name is an enumerator, not
/// text. `text` is the pseudo-tag of a text node and `root` of the one
/// synthetic node above the top-level blocks; the tag synonyms of the
/// input (`strong`, `em`, `del`, `strike`) are folded into their canonical
/// spelling by the parser and never appear here.
enum class tag_t : std::uint8_t {
    root,
    text,

    // Block-level.
    p,
    div,
    br,  // void; a line break is inline but carries block-like spacing rules
    h1,
    h2,
    h3,
    blockquote,
    ul,
    ol,
    li,
    pre,
    hr,  // void, yet a block: the rule stands between paragraphs, never inside one
    table,
    tr,
    td,
    th,
    details,  // a folded region; its first <summary> child is the visible header
    summary,

    // Inline.
    b,
    i,
    u,
    s,
    code,
    a,
    img,  // void
    sub,
    sup,
    font,
    span,  // no look of its own: exists to carry style="name" -- a highlighted piece of code
};

/// The attributes the subset admits (design.md, "Атрибуты"); anything else
/// in the input is dropped at the parse. `style` holds the *name* of a
/// registered style, never CSS.
enum class attr_t : std::uint8_t {
    href,
    src,
    width,
    height,
    alt,
    color,
    face,
    size,
    style,
    type,  // <ol>: 1, a, A, i, I -- the numbering, spelled as HTML spells it
    colspan,  // <td>/<th>
    rowspan,
};

/// One attribute of an element. The value is decoded (entities resolved)
/// and lives in the document's arena. The _t suffix is the house rule for
/// a type whose natural name node::attribute() already wears as a method.
class attribute_t {
public:
    constexpr attribute_t(attr_t name, std::wstring_view value) noexcept
        : name_(name), value_(value) {}

    constexpr attr_t name() const noexcept { return name_; }
    constexpr std::wstring_view value() const noexcept { return value_; }

private:
    attr_t name_;
    std::wstring_view value_;
};

/// The attributes of an element, in document order: one array in the arena.
using attribute_span = std::span<const attribute_t>;

/// A node of the tree.
///
/// Neither copyable nor movable, and not destroyed either: nodes are built
/// in the document's arena and released with it. The parser is the only
/// writer -- everything a consumer does is walk.
class node : public core::intrusive_slist_node<node> {
public:
    /// An element (or the root).
    inline explicit node(tag_t tag, attribute_span attributes = {}) noexcept
        : attributes_(attributes), tag_(tag) {}

    /// A text node: whitespace already collapsed, entities already decoded,
    /// and a zero right after the view -- see the header comment.
    inline explicit node(std::wstring_view text) noexcept : value_(text), tag_(tag_t::text) {}

    inline tag_t tag() const noexcept { return tag_; }
    inline bool is_text() const noexcept { return tag_ == tag_t::text; }
    inline bool is_element() const noexcept { return tag_ != tag_t::text; }

    /// What a text node holds; an element answers empty.
    constexpr std::wstring_view value() const noexcept { return value_; }

    inline const node* parent() const noexcept { return parent_; }

    /// The children, in document order.
    inline const node_list& children() const noexcept { return children_; }

    inline attribute_span attributes() const noexcept { return attributes_; }

    /// The text of that attribute; empty when the element does not carry it.
    ///
    /// Empty and absent are one thing here, and that is the rule rather than
    /// a shortcut: `<img src="">` is as much a missing source as `<img>`, and
    /// every caller that used to receive an optional went on to write
    /// `!x || x->empty()`. What is left of the distinction is `attribute()`
    /// below, for the caller that really is asking whether the element
    /// carries the thing at all.
    inline std::wstring_view attribute_str(attr_t name) const noexcept {
        for (const attribute_t& candidate : attributes_) {
            if (candidate.name() == name) return candidate.value();
        }
        return {};
    }

    /// The attribute itself, when the element carries it -- for the caller to
    /// whom its presence is the question and its text is not the answer.
    inline core::nullable<const attribute_t> attribute(attr_t name) const noexcept {
        for (const attribute_t& candidate : attributes_) {
            if (candidate.name() == name) return &candidate;
        }
        return nullptr;
    }

    /// Links `child` in as the last child. The parser's tool, public only
    /// because the parser lives in another translation unit of this module.
    inline void append_child(node& child) noexcept {
        children_.push_back(core::not_null<node>(&child));
        child.parent_ = this;
    }

private:
    node_list children_;
    const node* parent_ = nullptr;
    std::wstring_view value_;
    attribute_span attributes_;
    tag_t tag_;
};

/// Block-level, as the whitespace and auto-closing rules mean it. `br` is
/// not here: it breaks a line, not a block.
constexpr bool is_block(tag_t tag) noexcept {
    switch (tag) {
    case tag_t::p:
    case tag_t::div:
    case tag_t::h1:
    case tag_t::h2:
    case tag_t::h3:
    case tag_t::blockquote:
    case tag_t::ul:
    case tag_t::ol:
    case tag_t::li:
    case tag_t::pre:
    case tag_t::hr:
    case tag_t::table:
    case tag_t::tr:
    case tag_t::td:
    case tag_t::th:
    case tag_t::details:
    case tag_t::summary:
        return true;
    default:
        return false;
    }
}

/// Formatting inlines -- the ones a mismatched close reopens, the way
/// browsers do (design.md, "Ошибки разметки").
constexpr bool is_formatting(tag_t tag) noexcept {
    switch (tag) {
    case tag_t::b:
    case tag_t::i:
    case tag_t::u:
    case tag_t::s:
    case tag_t::code:
    case tag_t::a:
    case tag_t::sub:
    case tag_t::sup:
    case tag_t::font:
    case tag_t::span:
        return true;
    default:
        return false;
    }
}

/// Void elements: no content, no close tag.
constexpr bool is_void(tag_t tag) noexcept {
    return tag == tag_t::br || tag == tag_t::img || tag == tag_t::hr;
}

/// What a parser recovered from, for the diagnostic sink upstairs. None of
/// these ever fails a parse -- the recovery already happened when the record
/// was written; the record is for the developer wondering why the output is
/// not what the markup seemed to say. Shared by the whole family of markups:
/// BB and RSDN misnest their tags no less than HTML does.
enum class error_t : std::uint8_t {
    unknown_tag,      ///< tag outside the subset dropped, content kept
    unpaired_close,   ///< close tag with nothing open to match, ignored
    misnested_tags,   ///< close matched below the top; recovered browser-fashion
    bad_entity,       ///< `&name;`-shaped body that failed to decode, left literal
    misplaced_tag,    ///< a row or cell with no table around it, a summary outside details: dropped, content kept
};

/// One recovery record. The detail names the offender -- tag name, entity
/// body -- null-terminated like every other view the document hands out
/// and alive at least as long as the document (an arena copy or a
/// literal).
struct parse_error {
    error_t code;
    std::wstring_view detail;
};

/// The canonical spelling of a known tag, as a wide literal: what an error
/// record can point at forever, no arena copy needed.
constexpr std::wstring_view canonical_name(tag_t tag) noexcept {
    switch (tag) {
    case tag_t::p: return L"p";
    case tag_t::div: return L"div";
    case tag_t::br: return L"br";
    case tag_t::h1: return L"h1";
    case tag_t::h2: return L"h2";
    case tag_t::h3: return L"h3";
    case tag_t::blockquote: return L"blockquote";
    case tag_t::ul: return L"ul";
    case tag_t::ol: return L"ol";
    case tag_t::li: return L"li";
    case tag_t::pre: return L"pre";
    case tag_t::hr: return L"hr";
    case tag_t::table: return L"table";
    case tag_t::tr: return L"tr";
    case tag_t::td: return L"td";
    case tag_t::th: return L"th";
    case tag_t::details: return L"details";
    case tag_t::summary: return L"summary";
    case tag_t::b: return L"b";
    case tag_t::i: return L"i";
    case tag_t::u: return L"u";
    case tag_t::s: return L"s";
    case tag_t::code: return L"code";
    case tag_t::a: return L"a";
    case tag_t::img: return L"img";
    case tag_t::sub: return L"sub";
    case tag_t::sup: return L"sup";
    case tag_t::font: return L"font";
    case tag_t::span: return L"span";
    default: return {};
    }
}

}  // namespace wxl::html
