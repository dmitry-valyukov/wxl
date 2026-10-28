module wxl.core;

import std;

namespace wxl::json {
namespace {

// The parser's scratch containers allocate from the STA pool: the value stack
// grows and empties on every document, which is the repeated allocation the
// pool exists for.
template <typename T>
using sta_vector = std::vector<T, core::sta_allocator<T>>;

using sta_string = std::basic_string<char, std::char_traits<char>, core::sta_allocator<char>>;

/// A position in the document, one-based; the column counts bytes. It is
/// computed only on failure, so the parser keeps no line counter that the
/// successful path would pay for.
struct position {
    int line;
    int column;
};

position position_of(const char* const begin, const char* const where) noexcept {
    position at{1, 1};

    for (const char* scan = begin; scan < where; ++scan) {
        if (*scan == '\n') {
            ++at.line;
            at.column = 1;
        } else {
            ++at.column;
        }
    }

    return at;
}

constexpr bool is_space(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

constexpr bool is_digit(const char c) noexcept { return c >= '0' && c <= '9'; }

/// Parses one document from the buffer and leaves the tree in the arena.
///
/// Nothing here recurses: nesting is held by an explicit stack of frames, so a
/// document of a million opening brackets costs a million vector entries, not a
/// million call frames. The depth of a document is chosen by its author, not by
/// the code that reads it.
class parser {
public:
    parser(xml::arena& arena, const std::string_view text, const std::string_view file_name) noexcept
        : arena_(arena), begin_(text.data()), at_(text.data()), end_(text.data() + text.size()),
          file_name_(file_name) {}

    const value& parse();

private:
    /// An open container: its type, where its children begin in `values_`, and its
    /// own name. The position of the opening bracket lets the error for an unclosed
    /// container point at its beginning rather than at the end of the file.
    struct frame {
        value_type kind;
        std::size_t mark;
        core::u8_view name;
        const char* opened;
    };

    [[noreturn]] void fail(std::string_view reason, const char* where,
                           std::source_location rule = std::source_location::current()) const;

    void skip_space();
    void read_value();
    void close_frame();

    value read_scalar(core::u8_view name);
    core::u8_view read_field_name();
    core::u8_view read_string();
    double read_number();
    char32_t read_hex4();
    void expect_word(std::string_view word);

    /// Determines whether the end of the document has been reached.
    bool at_end() const noexcept { return at_ == end_; }

    xml::arena& arena_;

    const char* const begin_;
    const char* at_;
    const char* const end_;

    std::string_view file_name_;

    /// Values already parsed but not yet given to their parent. The children of one
    /// container are contiguous from `frame::mark`; closing the container copies that
    /// tail into the arena in one piece.
    sta_vector<value> values_;
    sta_vector<frame> frames_;

    /// A string that contains an escape sequence is assembled here and then copied
    /// into the arena.
    sta_string assembled_;
};

void parser::fail(const std::string_view reason, const char* const where,
                  const std::source_location rule) const {
    const position at = position_of(begin_, where);

    throw parsing_exception(file_name_, at.line, at.column, reason, rule);
}

// Skips white space and comments. JSON has no comments, yet they are always
// accepted: a comment cannot change the meaning of a document, only let through
// a document that would otherwise be rejected, and a document with `//` outside
// a string is invalid JSON in any case. A switch would be set the same
// way in every call and would catch no real error.
void parser::skip_space() {
    for (;;) {
        while (!at_end() && is_space(*at_)) ++at_;

        if (at_end() || *at_ != '/') return;

        const char* const opened = at_;

        if (at_ + 1 == end_) fail("expected a comment after '/'", opened);

        if (at_[1] == '/') {
            at_ += 2;
            while (!at_end() && *at_ != '\n') ++at_;
        } else if (at_[1] == '*') {
            at_ += 2;
            for (;;) {
                if (at_end()) fail("unterminated comment", opened);
                if (*at_ == '*' && at_ + 1 != end_ && at_[1] == '/') {
                    at_ += 2;
                    break;
                }
                ++at_;
            }
        } else {
            fail("expected a comment after '/'", opened);
        }
    }
}

char32_t parser::read_hex4() {
    if (end_ - at_ < 4) fail("expected four hexadecimal digits after '\\u'", at_ - 2);

    char32_t code = 0;

    for (int digit = 0; digit != 4; ++digit) {
        const char c = *at_++;

        code <<= 4;

        if (c >= '0' && c <= '9') code |= static_cast<char32_t>(c - '0');
        else if (c >= 'a' && c <= 'f') code |= static_cast<char32_t>(c - 'a' + 10);
        else if (c >= 'A' && c <= 'F') code |= static_cast<char32_t>(c - 'A' + 10);
        else fail("expected four hexadecimal digits after '\\u'", at_ - 1);
    }

    return code;
}

// A string without escape sequences is a view into the document, and nothing is
// copied. The first escape sequence switches to the slow path: it names
// characters not present in the document as bytes, so the string has to be
// assembled in the arena.
core::u8_view parser::read_string() {
    const char* const opened = at_;

    ++at_;  // past the opening quote

    const char* const from = at_;

    for (;;) {
        if (at_end()) fail("unterminated string", opened);

        const char c = *at_;

        if (c == '"') {
            const std::string_view whole(from, static_cast<std::size_t>(at_ - from));
            ++at_;
            return core::unicode::assume_valid(whole);
        }

        if (c == '\\') break;

        if (static_cast<unsigned char>(c) < 0x20)
            fail("unescaped control character in a string", at_);

        ++at_;
    }

    assembled_.assign(from, static_cast<std::size_t>(at_ - from));

    for (;;) {
        if (at_end()) fail("unterminated string", opened);

        const char c = *at_;

        if (c == '"') {
            ++at_;
            return core::unicode::assume_valid(arena_.copy(std::string_view(assembled_)));
        }

        if (static_cast<unsigned char>(c) < 0x20)
            fail("unescaped control character in a string", at_);

        if (c != '\\') {
            assembled_ += c;
            ++at_;
            continue;
        }

        const char* const escape = at_;

        ++at_;
        if (at_end()) fail("unterminated string", opened);

        switch (const char what = *at_++) {
        case '"':
        case '\\':
        case '/': assembled_ += what; break;
        case 'b': assembled_ += '\b'; break;
        case 'f': assembled_ += '\f'; break;
        case 'n': assembled_ += '\n'; break;
        case 'r': assembled_ += '\r'; break;
        case 't': assembled_ += '\t'; break;
        case 'u': {
            char32_t code = read_hex4();

            // A surrogate pair, such as the pair D83D DE00, encodes one character, and
            // its halves have meaning only together. The second escape is consumed only
            // when it holds a low surrogate; otherwise the document would lose the
            // escape sequence that follows.
            if (core::unicode::is_high_surrogate(code) && end_ - at_ >= 6 && at_[0] == '\\' && at_[1] == 'u') {
                const char* const pair = at_;

                at_ += 2;

                const char32_t low = read_hex4();

                if (core::unicode::is_low_surrogate(low))
                    code = 0x10000 + ((code - 0xD800) << 10) + (low - 0xDC00);
                else
                    at_ = pair;
            }

            // A lone surrogate cannot be encoded in UTF-8. It is a corrupt
            // character, not a grammar error, so instead of rejecting the document
            // it becomes U+FFFD, as in wxl::core::unicode::repaired.
            if (core::unicode::is_surrogate(code)) code = core::unicode::replacement_character;

            core::unicode::append_utf8(assembled_, code);
            break;
        }
        default: fail("unknown escape sequence", escape);
        }
    }
}

// A number: the JSON grammar finds its bounds, and wxl::core converts the text
// with std::from_chars, which does not depend on the locale, so the result is
// the same on a machine with any regional settings.
double parser::read_number() {
    const char* const from = at_;

    if (!at_end() && *at_ == '-') ++at_;

    if (at_end() || !is_digit(*at_)) fail("expected a digit after '-'", from);

    // JSON does not allow a leading zero: 007 is a typo, and reading it as 7
    // would hide it.
    if (*at_ == '0') ++at_;
    else while (!at_end() && is_digit(*at_)) ++at_;

    if (!at_end() && *at_ == '.') {
        ++at_;
        if (at_end() || !is_digit(*at_)) fail("expected digits after the decimal point", from);
        while (!at_end() && is_digit(*at_)) ++at_;
    }

    if (!at_end() && (*at_ == 'e' || *at_ == 'E')) {
        ++at_;
        if (!at_end() && (*at_ == '+' || *at_ == '-')) ++at_;
        if (at_end() || !is_digit(*at_)) fail("expected digits in the exponent", from);
        while (!at_end() && is_digit(*at_)) ++at_;
    }

    const std::string_view text(from, static_cast<std::size_t>(at_ - from));
    double number = 0;

    if (!wxl::core::try_parse(text, number)) fail("the number does not fit in a double", from);

    return number;
}

void parser::expect_word(const std::string_view word) {
    if (static_cast<std::size_t>(end_ - at_) < word.size() ||
        std::string_view(at_, word.size()) != word)
        fail(std::format("expected '{}'", word), at_);

    at_ += word.size();
}

value parser::read_scalar(const core::u8_view name) {
    const char c = at_end() ? '\0' : *at_;

    if (c == '"') return value::of_string(read_string(), name);
    if (c == 't') return expect_word("true"), value::of_boolean(true, name);
    if (c == 'f') return expect_word("false"), value::of_boolean(false, name);
    if (c == 'n') return expect_word("null"), value::of_null(name);
    if (c == '-' || is_digit(c)) return value::of_number(read_number(), name);

    fail(at_end() ? "the document ends where a value is expected" : "expected a value", at_);
}

core::u8_view parser::read_field_name() {
    if (at_end() || *at_ != '"') fail("expected a quoted member name", at_);

    const core::u8_view name = read_string();

    skip_space();

    if (at_end() || *at_ != ':') fail("expected ':' after the member name", at_);

    ++at_;

    return name;
}

// Closes a container: its children, contiguous in values_ from the frame's
// mark, are copied into the arena in one piece and replaced by one value, the
// container itself, ready to become a child of its parent.
void parser::close_frame() {
    const frame top = frames_.back();
    frames_.pop_back();

    const std::span<const value> children(values_.data() + top.mark, values_.size() - top.mark);
    const std::span<const value> kept = arena_.copy(children);

    values_.resize(top.mark);
    values_.push_back(value::of_items(top.kind, kept, top.name));
}

// Reads one value with everything nested in it. The outer loop opens values,
// the inner loop closes every container the value completes; the function
// returns when the last container is closed, that is, when the value is
// complete.
void parser::read_value() {
    core::u8_view pending_name;

    for (;;) {
        skip_space();

        const char c = at_end() ? '\0' : *at_;

        if (c == '{' || c == '[') {
            const bool object = c == '{';
            const char closer = object ? '}' : ']';

            frames_.push_back(frame{object ? value_type::object : value_type::array, values_.size(),
                                    pending_name, at_});
            pending_name = {};
            ++at_;

            skip_space();

            if (!at_end() && *at_ == closer) {
                ++at_;
                close_frame();
            } else {
                if (object) pending_name = read_field_name();
                continue;
            }
        } else {
            values_.push_back(read_scalar(pending_name));
            pending_name = {};
        }

        // The value is complete. A comma means that the next value follows; a
        // closing bracket closes the container, and the same check repeats for
        // the container.
        for (;;) {
            if (frames_.empty()) return;

            skip_space();

            const frame& top = frames_.back();
            const char closer = top.kind == value_type::object ? '}' : ']';

            if (at_end()) fail("unclosed object or array", top.opened);

            if (*at_ == ',') {
                ++at_;

                if (top.kind == value_type::object) {
                    skip_space();
                    pending_name = read_field_name();
                }
                break;
            }

            if (*at_ != closer)
                fail(std::format("expected ',' or '{}'", closer), at_);

            ++at_;
            close_frame();
        }
    }
}

const value& parser::parse() {
    skip_space();

    if (at_end()) fail("the document is empty", at_);

    read_value();

    skip_space();

    if (!at_end()) fail("the document does not end after the value", at_);

    // The root is copied into the arena: the scratch vector is destroyed with
    // the parser, and the tree has to outlive it, for as long as the document lives.
    return *arena_.create<value>(values_.back());
}

/// Reads the whole stream into one buffer, a megabyte per read, so that a large
/// document takes few reads.
std::string read_all(std::istream& source) {
    constexpr std::streamsize chunk = 1 << 20;

    std::string text;

    for (;;) {
        const std::size_t was = text.size();

        text.resize(was + static_cast<std::size_t>(chunk));
        source.read(text.data() + was, chunk);

        const std::streamsize got = source.gcount();

        text.resize(was + static_cast<std::size_t>(got));

        if (source.eof()) return text;

        if (source.bad() || (source.fail() && got != chunk))
            throw exception(std::format("the stream failed after {} bytes", text.size()));
    }
}

}  // namespace

const value& document::load(std::string&& source, const std::string_view file_name) {
    document_ = std::move(source);
    file_name_ = file_name;

    return parse_source();
}

const value& document::load(std::istream& source, const std::string_view file_name) {
    return load(read_all(source), file_name);
}

const value& document::load_file(const std::filesystem::path& path) {
    // A Windows file name is a sequence of 16-bit values in which an unpaired
    // surrogate is allowed. The name enters error messages repaired, not as is.
    std::string name(core::unicode::to_utf8(path).chars());

    std::ifstream file(path, std::ios::binary);

    if (!file) throw exception(std::format("cannot open {}", name));

    return load(file, name);
}

const value& document::parse_source() {
    // The previous tree is released whole: the arena frees its blocks at once,
    // without calling any destructor. An arena always holds a page and cannot be
    // emptied, so the old one is destroyed rather than cleared, and a new one is
    // created below, together with the parse that needs it.
    arena_.reset();
    root_ = nullptr;

    std::string_view text(document_);

    // A byte order mark: RFC 8259 allows a parser to ignore one, a file saved by an
    // editor may start with one, and in UTF-8 it states only the encoding.
    if (text.starts_with("\xEF\xBB\xBF")) text.remove_prefix(3);

    if (const core::nullable<std::size_t> broken = core::unicode::find_invalid_utf8(text)) {
        const position at = position_of(text.data(), text.data() + *broken);

        throw parsing_exception(file_name_, at.line, at.column, "invalid UTF-8",
                                std::source_location::current());
    }

    arena_ = std::make_unique<xml::arena>();

    parser reader(*arena_, text, file_name_);

    root_ = &reader.parse();

    return *root_;
}

}  // namespace wxl::json
