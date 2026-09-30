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

constexpr bool is_ws(char16_t c) noexcept {
    return c == u' ' || c == u'\t' || c == u'\r' || c == u'\n' || c == u'\f';
}

constexpr bool is_ascii_letter(char16_t c) noexcept {
    return (c >= u'a' && c <= u'z') || (c >= u'A' && c <= u'Z');
}

constexpr char16_t to_lower_ascii(char16_t c) noexcept {
    return c >= u'A' && c <= u'Z' ? static_cast<char16_t>(c + 32) : c;
}

// The tag of an element name (already lowercased), with the input's
// synonyms folded into their canonical spelling; nullopt for a name the
// subset does not know -- the tag then vanishes and its content stays.
std::optional<tag_t> element_tag(std::u16string_view name) noexcept {
    if (name == u"p") return tag_t::p;
    if (name == u"div") return tag_t::div;
    if (name == u"br") return tag_t::br;
    if (name == u"h1") return tag_t::h1;
    if (name == u"h2") return tag_t::h2;
    if (name == u"h3") return tag_t::h3;
    if (name == u"blockquote") return tag_t::blockquote;
    if (name == u"ul") return tag_t::ul;
    if (name == u"ol") return tag_t::ol;
    if (name == u"li") return tag_t::li;
    if (name == u"pre") return tag_t::pre;
    if (name == u"hr") return tag_t::hr;
    if (name == u"table") return tag_t::table;
    if (name == u"tr") return tag_t::tr;
    if (name == u"td") return tag_t::td;
    if (name == u"th") return tag_t::th;
    if (name == u"details") return tag_t::details;
    if (name == u"summary") return tag_t::summary;
    if (name == u"b" || name == u"strong") return tag_t::b;
    if (name == u"i" || name == u"em") return tag_t::i;
    if (name == u"u") return tag_t::u;
    if (name == u"s" || name == u"del" || name == u"strike") return tag_t::s;
    if (name == u"code") return tag_t::code;
    if (name == u"a") return tag_t::a;
    if (name == u"img") return tag_t::img;
    if (name == u"sub") return tag_t::sub;
    if (name == u"sup") return tag_t::sup;
    if (name == u"font") return tag_t::font;
    if (name == u"span") return tag_t::span;
    return std::nullopt;
}

std::optional<attr_t> attribute_name(std::u16string_view name) noexcept {
    if (name == u"href") return attr_t::href;
    if (name == u"src") return attr_t::src;
    if (name == u"width") return attr_t::width;
    if (name == u"height") return attr_t::height;
    if (name == u"alt") return attr_t::alt;
    if (name == u"color") return attr_t::color;
    if (name == u"face") return attr_t::face;
    if (name == u"size") return attr_t::size;
    if (name == u"style") return attr_t::style;
    if (name == u"type") return attr_t::type;
    if (name == u"colspan") return attr_t::colspan;
    if (name == u"rowspan") return attr_t::rowspan;
    return std::nullopt;
}

// The named entities of the subset (design.md: the full HTML set is
// deliberately not wanted). An unknown name stays literal text.
std::optional<char16_t> named_entity(std::u16string_view name) noexcept {
    if (name == u"amp") return u'&';
    if (name == u"lt") return u'<';
    if (name == u"gt") return u'>';
    if (name == u"quot") return u'"';
    if (name == u"apos") return u'\'';
    if (name == u"nbsp") return u' ';
    if (name == u"mdash") return u'—';
    if (name == u"ndash") return u'–';
    if (name == u"laquo") return u'«';
    if (name == u"raquo") return u'»';
    if (name == u"hellip") return u'…';
    if (name == u"copy") return u'©';
    return std::nullopt;
}

class parser {
public:
    parser(std::u16string_view input, tree_builder& out) : in_(input), out_(out) {}

    void run() {
        while (pos_ < in_.size()) {
            const char16_t c = in_[pos_];
            if (c == u'<' && markup()) continue;
            if (c == u'&') {
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
        const char16_t next = in_[pos_ + 1];

        if (next == u'!') {
            skip_declaration();
            return true;
        }
        if (next == u'?') {
            skip_until(u'>');
            return true;
        }
        if (next == u'/') {
            if (pos_ + 2 >= in_.size() || !is_ascii_letter(in_[pos_ + 2])) return false;
            pos_ += 2;
            const std::optional<tag_t> tag = element_tag(scan_name());
            skip_until(u'>');
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
            char16_t c = in_[pos_];
            if (c == u'>') {
                ++pos_;
                break;
            }
            if (c == u'/') {
                ++pos_;
                continue;
            }
            const std::optional<attr_t> name = attribute_name(scan_attribute_name());
            skip_ws();
            value_.clear();
            if (pos_ < in_.size() && in_[pos_] == u'=') {
                ++pos_;
                skip_ws();
                scan_attribute_value();
            }
            if (tag && name) attributes_.emplace_back(*name, out_.copy(value_));
        }

        if (!tag) return;  // the tag vanishes, its content stays
        out_.open(*tag, out_.copy_attributes(attributes_));
    }

    std::u16string_view scan_name() {
        name_.clear();
        while (pos_ < in_.size() &&
               (is_ascii_letter(in_[pos_]) || (in_[pos_] >= u'0' && in_[pos_] <= u'9'))) {
            name_ += to_lower_ascii(in_[pos_]);
            ++pos_;
        }
        return name_;
    }

    std::u16string_view scan_attribute_name() {
        name_.clear();
        while (pos_ < in_.size()) {
            const char16_t c = in_[pos_];
            if (is_ws(c) || c == u'=' || c == u'>' || c == u'/') break;
            name_ += to_lower_ascii(c);
            ++pos_;
        }
        return name_;
    }

    void scan_attribute_value() {
        if (pos_ >= in_.size()) return;
        const char16_t quote = in_[pos_];
        if (quote == u'"' || quote == u'\'') {
            ++pos_;
            while (pos_ < in_.size() && in_[pos_] != quote) value_char();
            if (pos_ < in_.size()) ++pos_;  // the closing quote
            return;
        }
        while (pos_ < in_.size() && !is_ws(in_[pos_]) && in_[pos_] != u'>') value_char();
    }

    void value_char() {
        if (in_[pos_] == u'&') {
            entity_into(value_);
            return;
        }
        value_ += in_[pos_];
        ++pos_;
    }

    void skip_declaration() {
        // <!-- ... --> as a unit; any other <!...> to the first '>'.
        if (in_.compare(pos_, 4, u"<!--") == 0) {
            const std::size_t end = in_.find(u"-->", pos_ + 4);
            pos_ = end == std::u16string_view::npos ? in_.size() : end + 3;
            return;
        }
        skip_until(u'>');
    }

    void skip_until(char16_t stop) {
        const std::size_t at = in_.find(stop, pos_);
        pos_ = at == std::u16string_view::npos ? in_.size() : at + 1;
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
            for (const char16_t c : entity_buf_) out_.character(c);
        } else {
            out_.character(u'&');
            ++pos_;
        }
    }

    void entity_into(core::sta_u16string& out) {
        entity_buf_.clear();
        if (decode_entity(entity_buf_)) {
            out += entity_buf_;
        } else {
            out += u'&';
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
    bool decode_entity(core::sta_u16string& out) {
        const std::size_t semicolon = in_.find(u';', pos_ + 1);
        if (semicolon == std::u16string_view::npos || semicolon == pos_ + 1 ||
            semicolon - pos_ > 32)
            return false;
        const std::u16string_view body = in_.substr(pos_ + 1, semicolon - pos_ - 1);

        if (body[0] == u'#') {
            std::uint32_t point = 0;
            if (!numeric_reference(body.substr(1), point)) return bad_entity(body);
            // Zero is rejected too: an embedded NUL inside a text piece is
            // a trap for every consumer that trusts the terminator.
            if (point == 0 || point > 0x10FFFF || (point >= 0xD800 && point <= 0xDFFF))
                return bad_entity(body);
            if (point >= 0x10000) {
                out += static_cast<char16_t>(0xD800 + ((point - 0x10000) >> 10));
                out += static_cast<char16_t>(0xDC00 + ((point - 0x10000) & 0x3FF));
            } else {
                out += static_cast<char16_t>(point);
            }
            pos_ = semicolon + 1;
            return true;
        }

        const std::optional<char16_t> named = named_entity(body);
        if (!named) return bad_entity(body);
        out += *named;
        pos_ = semicolon + 1;
        return true;
    }

    // The failed reference, recorded; false so the call reads as `return
    // bad_entity(body)` on the failure paths above.
    bool bad_entity(std::u16string_view body) {
        error_buf_.assign(body);
        out_.record_copied(error_t::bad_entity, error_buf_);
        return false;
    }

    static bool numeric_reference(std::u16string_view digits, std::uint32_t& point) {
        if (digits.empty()) return false;
        std::uint32_t base = 10;
        if (digits[0] == u'x' || digits[0] == u'X') {
            base = 16;
            digits.remove_prefix(1);
            if (digits.empty()) return false;
        }
        std::uint32_t value = 0;
        for (const char16_t c : digits) {
            std::uint32_t digit;
            if (c >= u'0' && c <= u'9') digit = c - u'0';
            else if (base == 16 && c >= u'a' && c <= u'f') digit = c - u'a' + 10;
            else if (base == 16 && c >= u'A' && c <= u'F') digit = c - u'A' + 10;
            else return false;
            value = value * base + digit;
            if (value > 0x110000) return false;  // clamp against overflow
        }
        point = value;
        return true;
    }

    std::u16string_view in_;
    std::size_t pos_ = 0;
    tree_builder& out_;

    // The scratch below grows once per parse -- but a parse runs per chat
    // message, so the allocations repeat and the pool is the right home
    // (unlike wxl.xml's per-document buffers, which stayed std:: for the
    // opposite reason).
    core::sta_u16string name_;
    core::sta_u16string value_;
    core::sta_u16string entity_buf_;
    core::sta_u16string error_buf_;
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

document parse(std::u16string_view input) {
    document_builder building;
    parser reader(input, building.tree());
    reader.run();
    return std::move(building).finish();
}

document parse(core::u8_view input) {
    const core::u16_text text = input.to_utf16();
    return parse(text.plain());
}

// ---- canonical serialization ----

namespace {

std::u16string_view serialized_attribute_name(attr_t name) noexcept {
    switch (name) {
    case attr_t::href: return u"href";
    case attr_t::src: return u"src";
    case attr_t::width: return u"width";
    case attr_t::height: return u"height";
    case attr_t::alt: return u"alt";
    case attr_t::color: return u"color";
    case attr_t::face: return u"face";
    case attr_t::size: return u"size";
    case attr_t::style: return u"style";
    case attr_t::type: return u"type";
    case attr_t::colspan: return u"colspan";
    case attr_t::rowspan: return u"rowspan";
    }
    return u"?";
}

void escape_into(std::u16string& out, std::u16string_view text) {
    for (const char16_t c : text) {
        switch (c) {
        case u'&': out += u"&amp;"; break;
        case u'<': out += u"&lt;"; break;
        case u'>': out += u"&gt;"; break;
        case u'"': out += u"&quot;"; break;
        default: out += c;
        }
    }
}

}  // namespace

std::u16string serialized(const node& root) {
    std::u16string out;

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
                out += u"</";
                out += canonical_name(leaving->tag());
                out += u'>';
            }
            continue;
        }
        const node& child = *top.child;
        ++top.child;

        if (child.is_text()) {
            escape_into(out, child.value());
            continue;
        }
        out += u'<';
        out += canonical_name(child.tag());
        for (const attribute_t& attr : child.attributes()) {
            out += u' ';
            out += serialized_attribute_name(attr.name());
            out += u"=\"";
            escape_into(out, attr.value());
            out += u'"';
        }
        out += u'>';
        if (is_void(child.tag())) continue;
        stack.push_back({&child, child.children().begin()});
    }
    return out;
}

}  // namespace wxl::html
