// The HTML tokenizer over the family's tree builder: one pass over the
// input, markup scanning and entities here, everything after a recognized
// tag -- the stack, the recovery, the whitespace -- in tree_builder
// (builder.cpp). The text is sacred, the tags are not.

module wxl.html;

import std;
import wxl.core;
import wxl.xml;

namespace wxl::html {

struct document::state {
    // One state per document, and a chat feed parses per message: the
    // allocation repeats, so it comes from the STA pool like everything
    // else here.
    static void* operator new(std::size_t size) {
        return core::sta_memory_pool::alloc(static_cast<std::uint32_t>(size));
    }

    static void operator delete(void* ptr, std::size_t size) noexcept {
        core::sta_memory_pool::free(ptr, static_cast<std::uint32_t>(size));
    }

    xml::arena arena;
    node* root = nullptr;
    xml::sta_vector<parse_error> errors;
};

namespace {

// The builder is pooled for the same reason the state is: one per parse,
// and parses repeat per chat message.
struct pooled_tree_builder : tree_builder {
    using tree_builder::tree_builder;

    static void* operator new(std::size_t size) {
        return core::sta_memory_pool::alloc(static_cast<std::uint32_t>(size));
    }

    static void operator delete(void* ptr, std::size_t size) noexcept {
        core::sta_memory_pool::free(ptr, static_cast<std::uint32_t>(size));
    }
};

constexpr bool is_ws(wchar_t c) noexcept {
    return c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\f';
}

constexpr bool is_ascii_letter(wchar_t c) noexcept {
    return (c >= L'a' && c <= L'z') || (c >= L'A' && c <= L'Z');
}

constexpr wchar_t to_lower_ascii(wchar_t c) noexcept {
    return c >= L'A' && c <= L'Z' ? static_cast<wchar_t>(c + 32) : c;
}

// The tag of an element name (already lowercased), with the input's
// synonyms folded into their canonical spelling; nullopt for a name the
// subset does not know -- the tag then vanishes and its content stays.
std::optional<tag_t> element_tag(std::wstring_view name) noexcept {
    if (name == L"p") return tag_t::p;
    if (name == L"div") return tag_t::div;
    if (name == L"br") return tag_t::br;
    if (name == L"h1") return tag_t::h1;
    if (name == L"h2") return tag_t::h2;
    if (name == L"h3") return tag_t::h3;
    if (name == L"blockquote") return tag_t::blockquote;
    if (name == L"ul") return tag_t::ul;
    if (name == L"ol") return tag_t::ol;
    if (name == L"li") return tag_t::li;
    if (name == L"pre") return tag_t::pre;
    if (name == L"hr") return tag_t::hr;
    if (name == L"table") return tag_t::table;
    if (name == L"tr") return tag_t::tr;
    if (name == L"td") return tag_t::td;
    if (name == L"th") return tag_t::th;
    if (name == L"details") return tag_t::details;
    if (name == L"summary") return tag_t::summary;
    if (name == L"b" || name == L"strong") return tag_t::b;
    if (name == L"i" || name == L"em") return tag_t::i;
    if (name == L"u") return tag_t::u;
    if (name == L"s" || name == L"del" || name == L"strike") return tag_t::s;
    if (name == L"code") return tag_t::code;
    if (name == L"a") return tag_t::a;
    if (name == L"img") return tag_t::img;
    if (name == L"sub") return tag_t::sub;
    if (name == L"sup") return tag_t::sup;
    if (name == L"font") return tag_t::font;
    if (name == L"span") return tag_t::span;
    return std::nullopt;
}

std::optional<attr_t> attribute_name(std::wstring_view name) noexcept {
    if (name == L"href") return attr_t::href;
    if (name == L"src") return attr_t::src;
    if (name == L"width") return attr_t::width;
    if (name == L"height") return attr_t::height;
    if (name == L"alt") return attr_t::alt;
    if (name == L"color") return attr_t::color;
    if (name == L"face") return attr_t::face;
    if (name == L"size") return attr_t::size;
    if (name == L"style") return attr_t::style;
    if (name == L"type") return attr_t::type;
    if (name == L"colspan") return attr_t::colspan;
    if (name == L"rowspan") return attr_t::rowspan;
    return std::nullopt;
}

// The named entities of the subset (design.md: the full HTML set is
// deliberately not wanted). An unknown name stays literal text.
std::optional<wchar_t> named_entity(std::wstring_view name) noexcept {
    if (name == L"amp") return L'&';
    if (name == L"lt") return L'<';
    if (name == L"gt") return L'>';
    if (name == L"quot") return L'"';
    if (name == L"apos") return L'\'';
    if (name == L"nbsp") return L' ';
    if (name == L"mdash") return L'—';
    if (name == L"ndash") return L'–';
    if (name == L"laquo") return L'«';
    if (name == L"raquo") return L'»';
    if (name == L"hellip") return L'…';
    if (name == L"copy") return L'©';
    return std::nullopt;
}

class parser {
public:
    parser(std::wstring_view input, tree_builder& out) : in_(input), out_(out) {}

    void run() {
        while (pos_ < in_.size()) {
            const wchar_t c = in_[pos_];
            if (c == L'<' && markup()) continue;
            if (c == L'&') {
                entity();
                continue;
            }
            out_.character(c);
            ++pos_;
        }
    }

private:
    // ---- markup scanning ----

    // At '<'. True when it was markup and pos_ moved past it; false when it
    // is just a character.
    bool markup() {
        if (pos_ + 1 >= in_.size()) return false;
        const wchar_t next = in_[pos_ + 1];

        if (next == L'!') {
            skip_declaration();
            return true;
        }
        if (next == L'?') {
            skip_until(L'>');
            return true;
        }
        if (next == L'/') {
            if (pos_ + 2 >= in_.size() || !is_ascii_letter(in_[pos_ + 2])) return false;
            pos_ += 2;
            const std::optional<tag_t> tag = element_tag(scan_name());
            skip_until(L'>');
            if (tag) out_.close(*tag);
            return true;
        }
        if (is_ascii_letter(next)) {
            ++pos_;
            open_tag();
            return true;
        }
        return false;
    }

    void open_tag() {
        const std::optional<tag_t> tag = element_tag(scan_name());
        // Recorded here, while name_ still holds the tag: the attribute
        // scan below reuses the same scratch.
        if (!tag) out_.record_copied(error_t::unknown_tag, name_);
        attributes_.clear();

        // Attributes are scanned whether the tag is known or not: an
        // unknown tag's `>` must be found past its quoted values, which may
        // hold a '>' of their own.
        for (;;) {
            skip_ws();
            if (pos_ >= in_.size()) break;
            wchar_t c = in_[pos_];
            if (c == L'>') {
                ++pos_;
                break;
            }
            if (c == L'/') {
                ++pos_;
                continue;
            }
            const std::optional<attr_t> name = attribute_name(scan_attribute_name());
            skip_ws();
            value_.clear();
            if (pos_ < in_.size() && in_[pos_] == L'=') {
                ++pos_;
                skip_ws();
                scan_attribute_value();
            }
            if (tag && name) attributes_.emplace_back(*name, out_.copy(value_));
        }

        if (!tag) return;  // the tag vanishes, its content stays
        out_.open(*tag, out_.copy_attributes(attributes_));
    }

    std::wstring_view scan_name() {
        name_.clear();
        while (pos_ < in_.size() &&
               (is_ascii_letter(in_[pos_]) || (in_[pos_] >= L'0' && in_[pos_] <= L'9'))) {
            name_ += to_lower_ascii(in_[pos_]);
            ++pos_;
        }
        return name_;
    }

    std::wstring_view scan_attribute_name() {
        name_.clear();
        while (pos_ < in_.size()) {
            const wchar_t c = in_[pos_];
            if (is_ws(c) || c == L'=' || c == L'>' || c == L'/') break;
            name_ += to_lower_ascii(c);
            ++pos_;
        }
        return name_;
    }

    void scan_attribute_value() {
        if (pos_ >= in_.size()) return;
        const wchar_t quote = in_[pos_];
        if (quote == L'"' || quote == L'\'') {
            ++pos_;
            while (pos_ < in_.size() && in_[pos_] != quote) value_char();
            if (pos_ < in_.size()) ++pos_;  // the closing quote
            return;
        }
        while (pos_ < in_.size() && !is_ws(in_[pos_]) && in_[pos_] != L'>') value_char();
    }

    void value_char() {
        if (in_[pos_] == L'&') {
            entity_into(value_);
            return;
        }
        value_ += in_[pos_];
        ++pos_;
    }

    void skip_declaration() {
        // <!-- ... --> as a unit; any other <!...> to the first '>'.
        if (in_.compare(pos_, 4, L"<!--") == 0) {
            const std::size_t end = in_.find(L"-->", pos_ + 4);
            pos_ = end == std::wstring_view::npos ? in_.size() : end + 3;
            return;
        }
        skip_until(L'>');
    }

    void skip_until(wchar_t stop) {
        const std::size_t at = in_.find(stop, pos_);
        pos_ = at == std::wstring_view::npos ? in_.size() : at + 1;
    }

    void skip_ws() {
        while (pos_ < in_.size() && is_ws(in_[pos_])) ++pos_;
    }

    // ---- character references ----

    // At '&' in text: decoded characters go through character(), so a
    // decoded space still collapses and a decoded '<' is just a character.
    void entity() {
        entity_buf_.clear();
        if (decode_entity(entity_buf_)) {
            for (const wchar_t c : entity_buf_) out_.character(c);
        } else {
            out_.character(L'&');
            ++pos_;
        }
    }

    void entity_into(xml::sta_wstring& out) {
        entity_buf_.clear();
        if (decode_entity(entity_buf_)) {
            out += entity_buf_;
        } else {
            out += L'&';
            ++pos_;
        }
    }

    // At '&'. On success appends the decoded character(s) and moves pos_
    // past the ';'; on failure leaves pos_ alone -- the caller writes the
    // literal '&' and steps over it, and the rest of the reference reads as
    // ordinary text. That is the whole error story: nothing throws.
    //
    // A bare '&' with no ';'-terminated body is *not* recorded as an error:
    // "Tom & Jerry" is prose, not a broken entity. Only a body that looked
    // like a reference and failed to decode is.
    bool decode_entity(xml::sta_wstring& out) {
        const std::size_t semicolon = in_.find(L';', pos_ + 1);
        if (semicolon == std::wstring_view::npos || semicolon == pos_ + 1 ||
            semicolon - pos_ > 32)
            return false;
        const std::wstring_view body = in_.substr(pos_ + 1, semicolon - pos_ - 1);

        if (body[0] == L'#') {
            std::uint32_t point = 0;
            if (!numeric_reference(body.substr(1), point)) return bad_entity(body);
            // Zero is rejected too: an embedded NUL inside a text piece is
            // a trap for every consumer that trusts the terminator.
            if (point == 0 || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
                return bad_entity(body);
            if (point >= 0x10000) {
                out += static_cast<wchar_t>(0xD800 + ((point - 0x10000) >> 10));
                out += static_cast<wchar_t>(0xDC00 + ((point - 0x10000) & 0x3FF));
            } else {
                out += static_cast<wchar_t>(point);
            }
            pos_ = semicolon + 1;
            return true;
        }

        const std::optional<wchar_t> named = named_entity(body);
        if (!named) return bad_entity(body);
        out += *named;
        pos_ = semicolon + 1;
        return true;
    }

    // The failed reference, recorded; false so the call reads as `return
    // bad_entity(body)` on the failure paths above.
    bool bad_entity(std::wstring_view body) {
        error_buf_.assign(body);
        out_.record_copied(error_t::bad_entity, error_buf_);
        return false;
    }

    static bool numeric_reference(std::wstring_view digits, std::uint32_t& point) {
        if (digits.empty()) return false;
        std::uint32_t base = 10;
        if (digits[0] == L'x' || digits[0] == L'X') {
            base = 16;
            digits.remove_prefix(1);
            if (digits.empty()) return false;
        }
        std::uint32_t value = 0;
        for (const wchar_t c : digits) {
            std::uint32_t digit;
            if (c >= L'0' && c <= L'9') digit = c - L'0';
            else if (base == 16 && c >= L'a' && c <= L'f') digit = c - L'a' + 10;
            else if (base == 16 && c >= L'A' && c <= L'F') digit = c - L'A' + 10;
            else return false;
            value = value * base + digit;
            if (value > 0x110000) return false;  // clamp against overflow
        }
        point = value;
        return true;
    }

    std::wstring_view in_;
    std::size_t pos_ = 0;
    tree_builder& out_;

    // The scratch below grows once per parse -- but a parse runs per chat
    // message, so the allocations repeat and the pool is the right home
    // (unlike wxl.xml's per-document buffers, which stayed std:: for the
    // opposite reason).
    xml::sta_wstring name_;
    xml::sta_wstring value_;
    xml::sta_wstring entity_buf_;
    xml::sta_wstring error_buf_;
    xml::sta_vector<attribute_t> attributes_;
};

}  // namespace

document::document(document&& other) noexcept : state_(other.state_) { other.state_ = nullptr; }

document& document::operator=(document&& other) noexcept {
    if (this != &other) {
        delete state_;
        state_ = other.state_;
        other.state_ = nullptr;
    }
    return *this;
}

document::~document() { delete state_; }

const node& document::root() const noexcept { return *state_->root; }

std::span<const parse_error> document::errors() const noexcept { return state_->errors; }

document_builder::document_builder()
    : state_(new document::state),
      tree_(new pooled_tree_builder(state_->arena, state_->errors)) {}

document_builder::~document_builder() {
    delete static_cast<pooled_tree_builder*>(tree_);
    delete state_;
}

document document_builder::finish() && noexcept {
    state_->root = &tree_->finish();
    document::state* state = state_;
    state_ = nullptr;
    delete static_cast<pooled_tree_builder*>(tree_);
    tree_ = nullptr;
    return document(state);
}

document parse(std::wstring_view input) {
    document_builder building;
    parser reader(input, building.tree());
    reader.run();
    return std::move(building).finish();
}

document parse(core::u8_view input) { return parse(input.to_utf16().wchars()); }

// ---- canonical serialization ----

namespace {

std::wstring_view serialized_attribute_name(attr_t name) noexcept {
    switch (name) {
    case attr_t::href: return L"href";
    case attr_t::src: return L"src";
    case attr_t::width: return L"width";
    case attr_t::height: return L"height";
    case attr_t::alt: return L"alt";
    case attr_t::color: return L"color";
    case attr_t::face: return L"face";
    case attr_t::size: return L"size";
    case attr_t::style: return L"style";
    case attr_t::type: return L"type";
    case attr_t::colspan: return L"colspan";
    case attr_t::rowspan: return L"rowspan";
    }
    return L"?";
}

void escape_into(std::wstring& out, std::wstring_view text) {
    for (const wchar_t c : text) {
        switch (c) {
        case L'&': out += L"&amp;"; break;
        case L'<': out += L"&lt;"; break;
        case L'>': out += L"&gt;"; break;
        case L'"': out += L"&quot;"; break;
        default: out += c;
        }
    }
}

}  // namespace

std::wstring serialized(const node& root) {
    std::wstring out;

    // Обход со своим стеком, не рекурсией: дерево глубиной в тысячи узлов --
    // законный результат разбора, и сериализатор обязан его пройти, а не
    // пережить (то же правило, что у самого парсера).
    struct level {
        const node* element;
        node_list::const_iterator child;
    };
    std::vector<level> stack;
    stack.push_back({&root, root.children().begin()});

    while (!stack.empty()) {
        level& top = stack.back();
        if (top.child == top.element->children().end()) {
            const node* leaving = top.element;
            stack.pop_back();
            if (!stack.empty()) {
                out += L"</";
                out += canonical_name(leaving->tag());
                out += L'>';
            }
            continue;
        }
        const node& child = *top.child;
        ++top.child;

        if (child.is_text()) {
            escape_into(out, child.value());
            continue;
        }
        out += L'<';
        out += canonical_name(child.tag());
        for (const attribute_t& attr : child.attributes()) {
            out += L' ';
            out += serialized_attribute_name(attr.name());
            out += L"=\"";
            escape_into(out, attr.value());
            out += L'"';
        }
        out += L'>';
        if (is_void(child.tag())) continue;
        stack.push_back({&child, child.children().begin()});
    }
    return out;
}

}  // namespace wxl::html
