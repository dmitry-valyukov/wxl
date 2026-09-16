module wxl.xml;

import std;
import :arena;
import wxl.core;

namespace wxl::xml {
namespace {

/// Pieces one attribute value starts out able to arrive in.
constexpr std::size_t initial_segments = 16;

/// Elements the stack of half-read elements starts out able to hold.
constexpr std::size_t initial_depth = 32;

/// Attributes of one element the two attribute buffers start out able to hold.
constexpr std::size_t initial_attributes = 16;

/// Namespace declarations the stack of those in scope starts out able to hold.
constexpr std::size_t initial_scopes = 16;


/// The character classes the grammar is written in terms of. Bytes are enough
/// for all of them: every character the grammar names is ASCII, and in UTF-8 an
/// ASCII byte never occurs inside a multi-byte sequence.
/// @{
constexpr bool is_space(const char c) noexcept {
    return c == ' ' || c == '\t' || c == '\r' || c == '\n';
}

constexpr bool is_alpha(const char c) noexcept {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z');
}

constexpr bool is_digit(const char c) noexcept {
    return c >= '0' && c <= '9';
}

constexpr bool is_hex_digit(const char c) noexcept {
    return is_digit(c) || (c >= 'a' && c <= 'f') || (c >= 'A' && c <= 'F');
}

constexpr char32_t hex_digit_value(const char c) noexcept {
    if (is_digit(c))
        return static_cast<char32_t>(c - '0');

    return static_cast<char32_t>((c >= 'a' && c <= 'f') ? c - 'a' + 10 : c - 'A' + 10);
}

/// NameChar without the combining characters and extenders: [a-zA-Z0-9_.:-].
constexpr bool is_name_char(const char c) noexcept {
    return is_alpha(c) || is_digit(c) || c == '_' || c == '.' || c == ':' || c == '-';
}

constexpr char low_case(const char c) noexcept {
    return (c >= 'A' && c <= 'Z') ? static_cast<char>(c - 'A' + 'a') : c;
}

/// Char, the set a document may be built from at all. A byte with the high bit
/// set belongs to a sequence the validation pass has accepted, so only the
/// ASCII controls are refused here.
constexpr bool is_xml_char(const char c) noexcept {
    return c == 0x9 || c == 0xa || c == 0xd || static_cast<unsigned char>(c) >= 0x20;
}
/// @}

/// The classes above as bits, so a rule can ask about a byte with one load and
/// one test. This is what the scanning loops are written in terms of.
enum char_class : unsigned char {
    space = 1,                                              ///< S
    name_start = 2,                                          ///< What a Name may begin with
    name_char = 4,                                           ///< What a Name may go on with
    content_break = 8,                                       ///< What ends a run of character data
    value_break = 16,                                        ///< What ends a run inside an attribute value
};

/// One entry per byte. A byte above 0x7F is in no class at all, which is what
/// makes every rule treat it as ordinary content.
///
/// The bytes Char excludes break both runs, so a scan stops on one and the
/// rule standing there refuses it -- one comparison where the scan stopped
/// rather than one per byte. The document's terminating zero is such a byte,
/// and that is what ends every scan without any of them carrying a bound.
constexpr std::array<unsigned char, 256> char_classes = [] {
    std::array<unsigned char, 256> classes{};

    for (int byte = 0; byte < 0x80; ++byte) {
        const auto c = static_cast<char>(byte);
        unsigned char bits = 0;

        if (is_space(c))
            bits |= space;

        if (is_alpha(c) || c == '_' || c == ':')
            bits |= name_start;

        if (is_name_char(c))
            bits |= name_char;

        if (c == '<' || c == '&' || c == ']' || !is_xml_char(c))
            bits |= content_break;

        if (c == '<' || c == '&' || c == '"' || c == '\'' || !is_xml_char(c))
            bits |= value_break;

        classes[static_cast<std::size_t>(byte)] = bits;
    }

    return classes;
}();

constexpr bool is_class(const char c, const char_class wanted) noexcept {
    return (char_classes[static_cast<unsigned char>(c)] & wanted) != 0;
}

/// The five references a document may use without declaring them.
struct entity {
    std::string_view name;
    char character;
};

constexpr std::array predefined_entities = {
    entity{"lt", '<'},
    entity{"gt", '>'},
    entity{"amp", '&'},
    entity{"apos", '\''},
    entity{"quot", '"'},
};
}  // namespace

// ---- bookmark ----

std::string_view parser::bookmark::substring(const std::size_t end_skip) const noexcept {
    const auto length = static_cast<std::size_t>(parser_.at_ - position_);
    return {position_, length - std::min(length, end_skip)};
}

// ---- Where the parser stands ----

bool parser::looks_like(const std::string_view text) const noexcept {
    // No text a rule looks for holds a zero, so a comparison reaching the
    // terminator fails there rather than reading past the end.
    for (std::size_t index = 0; index != text.size(); ++index) {
        if (at_[index] != text[index])
            return false;
    }

    return true;
}

parser::line_column parser::place(const char* const position) noexcept {
    // Backwards only when a rule that already failed asks where it began.
    // Counting again from the top is correct, and rare enough to be cheaper
    // than a second bookkeeping to avoid it.
    if (position < place_at_) {
        place_at_ = start_;
        place_line_ = start_;
        place_number_ = 1;
    }

    while (place_at_ != position) {
        const auto length = static_cast<std::size_t>(position - place_at_);
        const auto* const newline = static_cast<const char*>(std::memchr(place_at_, '\n', length));

        if (newline == nullptr)
            break;

        place_line_ = newline + 1;
        place_at_ = place_line_;
        ++place_number_;
    }

    place_at_ = position;

    return {place_number_, static_cast<int>(position - place_line_) + 1};
}

void parser::syntax_error(const std::source_location& rule) {
    syntax_error(at_, rule);
}

void parser::syntax_error(const char* const position, const std::source_location& rule) {
    const line_column stopped = place(position);

    throw parsing_exception(file_name_, stopped.line, stopped.column, rule);
}

// ---- The pieces a value is collected in ----

parser::parser(const std::string_view source, arena& into, const options& opts,
               const std::string_view file_name)
    : arena_(into),
      options_(opts),
      file_name_(file_name),
      at_(source.data()),
      start_(source.data()),
      place_at_(source.data()),
      place_line_(source.data()) {
    // Room for the shapes a document usually has, taken once here rather than
    // grown a piece at a time during the parse. Five small allocations per
    // document, which is what a parser on the stack costs against one kept as
    // a member -- and against the parse they pay for, nothing at all.
    segments_.reserve(initial_segments);
    stack_.reserve(initial_depth);
    attributes_.reserve(initial_attributes);
    raw_attributes_.reserve(initial_attributes);
    scopes_.reserve(initial_scopes);
}

node& parser::parse() {
    // parse_document() answers with a pointer because its own rules do; it
    // never answers with none, having thrown instead.
    return *parse_document();
}

void parser::add_segment(const std::string_view piece) {
    // An empty piece is dropped, so a value the document does not really
    // interrupt still counts as the single piece it is.
    if (!piece.empty())
        segments_.push_back(piece);
}

std::string_view parser::take_segments() {
    const std::span<const std::string_view> pieces{segments_};
    std::string_view value;

    if (pieces.size() == 1) {
        // The ordinary case, and the reason the pieces are collected rather
        // than appended: the value is already there, whole.
        value = pieces.front();
    } else if (!pieces.empty()) {
        std::size_t length = 0;

        for (const std::string_view piece : pieces)
            length += piece.size();

        const std::span<char> joined = arena_.chars(length);
        char* at = joined.data();

        for (const std::string_view piece : pieces)
            at = std::copy(piece.begin(), piece.end(), at);

        value = {joined.data(), joined.size()};
    }

    segments_.clear();
    return value;
}

void parser::push(node& el, const char* const opening, const std::size_t scope_mark) {
    stack_.push_back(frame{.el = &el, .opening = opening, .from = at_, .scope_mark = scope_mark});
}

void parser::add_text(const std::string_view piece, const char* const position,
                                node& parent) {
    if (!options_.keep_text || piece.empty())
        return;

    // Asked for in document order, like every other place(): the piece begins
    // before whatever ends it, and nothing behind it has been placed yet.
    const line_column at = place(position);

    xml::node* const created = arena_.create<xml::node>(node_type::text, qualified_name{}, at.line, at.column);
    created->set_value(core::assume_valid(piece));
    parent.add_child(*created);
}

// ---- Namespaces ----

namespace {

/// The two namespaces XML binds itself, which no document has to declare and
/// none may rebind.
constexpr std::string_view xml_prefix = "xml";
constexpr std::string_view xmlns_prefix = "xmlns";

// The URIs go into the tree, so they are checked text -- and being literals,
// checked by the compiler where they stand rather than by anything run.
constexpr core::u8_view xml_namespace = u8"http://www.w3.org/XML/1998/namespace";
constexpr core::u8_view xmlns_namespace = u8"http://www.w3.org/2000/xmlns/";

}  // namespace

// Everything below hands pieces of the document to the tree, which holds
// checked text -- so this is where the guarantee is attached, and assume_valid()
// is the word for it. The claim is the same one in all four places: the buffer
// was walked whole by validate_utf8() before the grammar started, and the
// grammar cuts only at the characters XML's syntax names. All of those are
// ASCII, and an ASCII byte never stands inside a UTF-8 sequence, so a piece of
// a well-formed document is well-formed itself.

core::u8_view parser::namespace_for(const std::string_view prefix) const noexcept {
    // Backwards: the innermost declaration of a prefix is the one in force.
    for (const namespace_binding& binding : scopes_ | std::views::reverse)
        if (binding.prefix == prefix)
            return core::assume_valid(binding.uri);

    if (prefix == xml_prefix)
        return xml_namespace;

    if (prefix == xmlns_prefix)
        return xmlns_namespace;

    // A prefix nothing declared. Refusing the document here would be the
    // letter of the recommendation, and would also mean a book no reader can
    // open because one attribute of it names a vocabulary nobody uses. The
    // name keeps its prefix and stands in no namespace, which is exactly what
    // the document said.
    return {};
}

qualified_name parser::resolve(const std::string_view qualified,
                                        const bool is_attribute) const noexcept {
    const std::size_t colon = qualified.find(':');
    const core::u8_view name = core::assume_valid(qualified);

    if (colon == std::string_view::npos) {
        if (is_attribute) {
            // The default namespace covers elements only: an unprefixed
            // attribute belongs to the element that carries it, not to a
            // vocabulary. The declaration itself is the exception -- `xmlns`
            // is a name XML bound, and it stands in the xmlns namespace
            // exactly as `xmlns:p` does.
            return {name, 0, qualified == xmlns_prefix ? xmlns_namespace : core::u8_view{}};
        }

        return {name, 0, namespace_for({})};
    }

    return {name, colon, namespace_for(qualified.substr(0, colon))};
}

std::size_t parser::open_scope() {
    const std::size_t mark = scopes_.size();

    // Declarations first and all of them: they hold for the element itself and
    // for every attribute of it, whatever order they were written in.
    for (const raw_attribute& attribute : raw_attributes_) {
        if (attribute.name == xmlns_prefix)
            scopes_.push_back({{}, attribute.value});
        else if (attribute.name.starts_with("xmlns:"))
            scopes_.push_back({attribute.name.substr(xmlns_prefix.size() + 1), attribute.value});
    }

    return mark;
}

// ---- The grammar ----

bool parser::parse_string(const std::string_view text) noexcept {
    if (!looks_like(text))
        return false;

    at_ += text.size();
    return true;
}

bool parser::parse_string_no_case(const std::string_view text) noexcept {
    for (std::size_t index = 0; index != text.size(); ++index) {
        if (low_case(at_[index]) != low_case(text[index]))
            return false;
    }

    at_ += text.size();
    return true;
}

bool parser::parse_number(char32_t& number) noexcept {
    if (!is_digit(peek()))
        return false;

    number = 0;

    for (char c = peek(); is_digit(c); c = peek()) {
        // Saturating rather than wrapping: the caller refuses a code point
        // this large, and it must not become a small one on the way there.
        if (number <= wxl::core::max_code_point)
            number = number * 10 + static_cast<char32_t>(c - '0');

        advance();
    }

    return true;
}

bool parser::parse_hex_number(char32_t& number) noexcept {
    if (!is_hex_digit(peek()))
        return false;

    number = 0;

    for (char c = peek(); is_hex_digit(c); c = peek()) {
        if (number <= wxl::core::max_code_point)
            number = number * 16 + hex_digit_value(c);

        advance();
    }

    return true;
}

bool parser::parse_char(const char character) noexcept {
    if (peek() != character)
        return false;

    advance();
    return true;
}

/// `S ::= (#x20 | #x9 | #xD | #xA)+`
bool parser::parse_spaces() noexcept {
    const char* const from = at_;

    while (is_class(*at_, space))
        ++at_;

    return at_ != from;
}

/// `Name ::= (Letter | '_' | ':') NameChar*`
bool parser::parse_name(std::string_view& name) noexcept {
    if (!is_class(*at_, name_start))
        return false;

    const char* const from = at_;

    do {
        ++at_;
    } while (is_class(*at_, name_char));

    name = {from, static_cast<std::size_t>(at_ - from)};
    return true;
}

/// The opening of a declaration: `S <keyword> Eq`.
bool parser::parse_declaration_opening(const std::string_view declaration) {
    // Restored on failure, so an optional declaration that turns out not to be
    // there costs the caller nothing -- not even the spaces before it.
    const bookmark mark(*this);

    if (!parse_spaces() || !parse_string(declaration)) {
        mark.restore();
        return false;
    }

    if (!parse_eq())
        syntax_error();

    return true;
}

/// `XMLDecl ::= '<?xml' VersionInfo EncodingDecl? SDDecl? S? '?>'`
///
/// Matched exactly, lower case, with a space behind it: `xml-stylesheet` is an
/// ordinary instruction, and a prefix match would take it for a declaration
/// and then refuse the document over a version it never claimed.
bool parser::parse_xml_declaration() {
    if (!looks_like("<?xml") || !is_class(at_[5], space))
        return false;

    at_ += 5;

    std::string_view version;

    if (!parse_version_info(version))
        syntax_error();

    xml_version_ = core::assume_valid(version);

    parse_encoding_decl();                                    // optional
    parse_standalone_declaration();                           // optional

    parse_spaces();

    if (!parse_string("?>"))
        syntax_error();

    return true;
}

/// `doctypedecl ::= '<!DOCTYPE' S Name (S ExternalID)? S? ('[' intSubset ']' S?)? '>'`
///
/// Read to its end and dropped: this reader has no DTD, so an entity a
/// document declares for itself is refused where it is used. Read rather than
/// run to the first '>', which a quoted identifier may hold and an internal
/// subset certainly does.
bool parser::parse_xml_doctype() {
    const char* const opening = at_;

    if (!parse_string_no_case("<!DOCTYPE"))
        return false;

    for (;;) {
        switch (const char c = next_char()) {
        case '>':
            return true;

        case '"':
        case '\'':
            skip_quoted(c);
            break;

        case '[':
            skip_internal_subset();
            break;

        case '\0':
            syntax_error(opening);

        default:
            break;
        }
    }
}

void parser::skip_quoted(const char quote) {
    const char* const opening = at_ - 1;                    // the quote itself

    for (;;) {
        const char c = next_char();

        if (c == quote)
            return;

        if (c == '\0')
            syntax_error(opening);
    }
}

/// intSubset -- the declarations a document writes between '[' and ']'. Only
/// the quoted strings and the comments inside have to be recognised, since
/// either may hold a ']' that ends nothing.
void parser::skip_internal_subset() {
    const char* const opening = at_ - 1;                    // the '[' itself

    for (;;) {
        if (std::string_view comment; parse_comment(comment))
            continue;

        const char c = next_char();

        if (c == ']')
            return;

        if (c == '"' || c == '\'') {
            skip_quoted(c);
            continue;
        }

        if (c == '\0')
            syntax_error(opening);
    }
}

/// `Eq ::= S? '=' S?`
bool parser::parse_eq() noexcept {
    const bookmark mark(*this);

    parse_spaces();

    if (!parse_char('=')) {
        mark.restore();
        return false;
    }

    parse_spaces();
    return true;
}

/// `VersionInfo ::= S 'version' Eq ("'" VersionNum "'" | '"' VersionNum '"')`
bool parser::parse_version_info(std::string_view& version) {
    if (!parse_declaration_opening("version"))
        return false;

    const char quote = next_char();

    if (quote != '\'' && quote != '"')
        syntax_error();

    if (!parse_version_num(version) || next_char() != quote)
        syntax_error();

    return true;
}

/// `VersionNum ::= ([a-zA-Z0-9_.:] | '-')+`
bool parser::parse_version_num(std::string_view& version) noexcept {
    const char* const from = at_;

    while (is_class(*at_, name_char))
        ++at_;

    if (at_ == from)
        return false;

    version = {from, static_cast<std::size_t>(at_ - from)};
    return true;
}

/// `SDDecl ::= S 'standalone' Eq (("'" ('yes' | 'no') "'") | ('"' ('yes' | 'no') '"'))`
bool parser::parse_standalone_declaration() {
    if (!parse_declaration_opening("standalone"))
        return false;

    const char quote = next_char();

    if (quote != '\'' && quote != '"')
        syntax_error();

    if (!parse_string_no_case("yes") && !parse_string_no_case("no"))
        syntax_error();

    if (next_char() != quote)
        syntax_error();

    return true;
}

/// `EncodingDecl ::= S 'encoding' Eq ('"' EncName '"' | "'" EncName "'")`
///
/// The name is read and dropped: this reader parses UTF-8, so what a document
/// says about its own bytes is not something it can act on.
bool parser::parse_encoding_decl() {
    if (!parse_declaration_opening("encoding"))
        return false;

    const char quote = next_char();

    if (quote != '\'' && quote != '"')
        syntax_error();

    if (!parse_enc_name() || next_char() != quote)
        syntax_error();

    return true;
}

/// `EncName ::= [A-Za-z] ([A-Za-z0-9._] | '-')*`
bool parser::parse_enc_name() noexcept {
    if (!is_alpha(*at_))
        return false;

    do {
        ++at_;
    } while (is_class(*at_, name_char));

    return true;
}

/// `Misc* ::= (Comment | PI | S)*`
///
/// What stands between top-level markup belongs to no element, so all of it is
/// read and dropped.
void parser::parse_miscs() {
    for (;;) {
        parse_spaces();

        if (std::string_view comment; parse_comment(comment))
            continue;

        if (!parse_pi())
            return;
    }
}

/// `PI       ::= '<?' PITarget (S (Char* - (Char* '?>' Char*)))? '?>'`
/// `PITarget ::= Name - (('X' | 'x') ('M' | 'm') ('L' | 'l'))`
///
/// Read and dropped: an instruction is addressed to an application that knows
/// what to do with it, and this reader is not one.
bool parser::parse_pi() {
    const char* const opening = at_;

    if (!parse_string("<?"))
        return false;

    if (std::string_view target; !parse_name(target))
        syntax_error();

    while (!parse_string("?>")) {
        if (next_char() == '\0')
            syntax_error(opening);
    }

    return true;
}

/// `Reference ::= EntityRef | CharRef`
/// `EntityRef ::= '&' Name ';'`
/// `CharRef   ::= '&#' [0-9]+ ';' | '&#x' [0-9a-fA-F]+ ';'`
///
/// What a reference stands for appears nowhere in the document as it reads, so
/// unlike every other piece of a value it cannot be a view into it.
std::string_view parser::parse_reference_text() {
    const char* const opening = at_;

    if (!parse_char('&'))
        syntax_error();

    if (!parse_char('#')) {
        std::string_view name;

        if (!parse_name(name) || !parse_char(';'))
            syntax_error(opening);

        const auto entity = std::ranges::find(predefined_entities, name, &entity::name);

        if (entity == predefined_entities.end())
            syntax_error(opening);

        // The table itself: these five bytes live as long as the program, so a
        // copy per occurrence would be a copy for nothing.
        return {&entity->character, 1};
    }

    char32_t code_point = 0;

    if (parse_char('x')) {
        if (!parse_hex_number(code_point))
            syntax_error(opening);
    } else if (!parse_number(code_point)) {
        syntax_error(opening);
    }

    if (!parse_char(';') || code_point > wxl::core::max_code_point ||
        wxl::core::is_surrogate(code_point))
        syntax_error(opening);

    // Four bytes at the most, which a string holds without allocating; the
    // arena is what makes the copy last as long as the tree pointing at it.
    std::string bytes;
    wxl::core::append_utf8(bytes, code_point);

    return arena_.copy(bytes);
}

/// `AttValue ::= '"' ([^<&"] | Reference)* '"' | "'" ([^<&'] | Reference)* "'"`
bool parser::parse_attribute_value(std::string_view& value) {
    const char quote = peek();

    if (quote != '\'' && quote != '"')
        return false;

    const char* const opening = at_;

    advance();

    const char* from = at_;

    for (;;) {
        while (!is_class(*at_, value_break))
            ++at_;

        const char c = *at_;

        if (c == quote)
            break;

        // The other quote, which inside this one is ordinary text. One class
        // covers both, so telling them apart costs a comparison per value
        // rather than an argument to the scan above.
        if (c == '\'' || c == '"') {
            ++at_;
            continue;
        }

        // A '\0' says the document ended, so the value that never closed is
        // what to look at; everything else is wrong where it stands.
        if (c != '&')
            syntax_error(c == '\0' ? opening : at_);

        add_segment({from, static_cast<std::size_t>(at_ - from)});
        add_segment(parse_reference_text());
        from = at_;
    }

    add_segment({from, static_cast<std::size_t>(at_ - from)});
    advance();                                              // the closing quote

    value = take_segments();
    return true;
}

/// `attribute ::= Name Eq AttValue`
///
/// Collected into the parser's own array; the element takes the finished lot
/// once its start tag is over.
bool parser::parse_attribute() {
    const char* const opening = at_;
    std::string_view name;

    if (!parse_name(name))
        return false;

    // A start tag may not name the same attribute twice. Quadratic in the
    // attributes of one element, which is under two on the documents this
    // reads.
    if (std::ranges::any_of(raw_attributes_, [name](const raw_attribute& other) { return other.name == name; }))
        syntax_error(opening);

    std::string_view value;

    if (!parse_eq() || !parse_attribute_value(value))
        syntax_error(opening);

    raw_attributes_.emplace_back(name, value);
    return true;
}

/// `CDSect ::= '<![CDATA[' (Char* - (Char* ']]>' Char*)) ']]>'`
bool parser::parse_cdata(std::string_view& text) {
    const char* const opening = at_;

    if (!parse_string("<![CDATA["))
        return false;

    const bookmark mark(*this);

    while (!parse_string("]]>")) {
        if (peek() == '\0')
            syntax_error(opening);

        if (!is_xml_char(peek()))
            syntax_error();

        advance();
    }

    // Taken as it stands: inside CDATA nothing is a reference, so the section
    // is one slice of the document like any other piece of content.
    text = mark.substring(3);
    return true;
}

/// `Comment ::= '<!--' ((Char - '-') | ('-' (Char - '-')))* '-->'`
///
/// Hands back the text between the delimiters; whether that becomes a node is
/// the caller's business, so nothing is built here that might be thrown away.
bool parser::parse_comment(std::string_view& text) {
    const char* const opening = at_;

    if (!parse_string("<!--"))
        return false;

    const bookmark mark(*this);

    for (;;) {
        if (parse_string("--")) {
            if (!parse_char('>'))
                syntax_error();

            break;
        }

        if (next_char() == '\0')
            syntax_error(opening);
    }

    text = mark.substring(3);
    return true;
}

/// `STag ::= '<' Name (S attribute)* S?`
node* parser::parse_start_tag(std::size_t& scope_mark) {
    // Taken before the '<', which is where the element reports itself.
    const bookmark start(*this);

    if (!parse_char('<'))
        return nullptr;

    std::string_view name;

    if (!parse_name(name))
        syntax_error();

    const line_column at = start.place();

    raw_attributes_.clear();

    while (parse_spaces() && parse_attribute())
        ;

    scope_mark = open_scope();

    node* const el = arena_.create<node>(node_type::element, resolve(name, false), at.line, at.column);

    attributes_.clear();
    attributes_.reserve(raw_attributes_.size());

    for (const raw_attribute& attribute : raw_attributes_)
        attributes_.emplace_back(resolve(attribute.name, true), core::assume_valid(attribute.value));

    el->set_attributes(arena_.copy(std::span<const attribute>(attributes_)));
    return el;
}

/// `ETag ::= '</' Name S? '>'`
void parser::parse_etag(const node& el, const char* const opening) {
    if (!parse_string("</"))
        syntax_error();

    std::string_view name;

    if (!parse_name(name))
        syntax_error();

    // Compared as written, not as resolved: XML matches an end tag against the
    // start tag byte for byte, prefix included.
    if (name != el.qualified_name())
        syntax_error(opening);

    parse_spaces();

    if (!parse_char('>'))
        syntax_error();
}

/// EmptyElemTag against STag: '/>' with nothing behind it, or '>' with the
/// element's content.
bool parser::has_content() {
    const char* const stopped = at_;
    const char c = next_char();

    if (c == '/') {
        if (next_char() != '>')
            syntax_error(at_ - 1);

        return false;
    }

    if (c != '>')
        syntax_error(stopped);

    return true;
}

/// `element ::= EmptyElemTag | STag content ETag`
/// `content ::= (element | CharData | Reference | CDSect | PI | Comment)*`
///
/// One loop over a stack of half-read elements, rather than a rule calling
/// itself through the content it reads. What a document may nest is then
/// bounded by memory, like everything else about it, instead of by the stack
/// the process happens to have been given -- where the bound is reached
/// without warning, cannot be caught, and takes the process with it.
node* parser::parse_element() {
    const char* const opening = at_;
    std::size_t scope_mark = 0;
    node* const root = parse_start_tag(scope_mark);

    if (root == nullptr)
        return nullptr;

    if (has_content())
        push(*root, opening, scope_mark);
    else
        scopes_.resize(scope_mark);

    while (!stack_.empty()) {
        // Valid until something is pushed, which the two places that do it are
        // written around.
        frame& f = stack_.back();

        // Indentation first and on its own: between an element's children
        // there is usually nothing else, and close to a third of a real
        // document is this, walked by the shorter of the two scans.
        while (is_class(*at_, space))
            ++at_;

        for (;;) {
            while (!is_class(*at_, content_break))
                ++at_;

            if (*at_ != ']')
                break;

            // ']]>' ends a CDATA section and may appear nowhere else.
            if (looks_like("]]>"))
                syntax_error();

            ++at_;
        }

        const char c = *at_;

        if (c != '<' && c != '&')
            syntax_error(c == '\0' ? f.opening : at_);

        // Everything read so far is one piece of the document; whatever
        // interrupts it here is not part of it, and starts a piece of its own.
        add_text({f.from, static_cast<std::size_t>(at_ - f.from)}, f.from, *f.el);

        if (c == '&') {
            const char* const reference_opening = at_;

            add_text(parse_reference_text(), reference_opening, *f.el);
            continue_text(f);
            continue;
        }

        // What a '<' opens is decided by the byte behind it, which is always
        // there to read: a document ending on a '<' was refused just above.
        switch (at_[1]) {
        case '/': {
            parse_etag(*f.el, f.opening);

            // What the element declared stops here, with it.
            scopes_.resize(f.scope_mark);
            stack_.pop_back();

            if (!stack_.empty())
                continue_text(stack_.back());

            continue;
        }

        case '!': {
            const char* const comment_opening = at_;

            if (std::string_view comment; parse_comment(comment)) {
                if (options_.keep_comments) {
                    const line_column at = place(comment_opening);

                    xml::node* const created =
                        arena_.create<xml::node>(node_type::comment, qualified_name{}, at.line, at.column);
                    created->set_value(core::assume_valid(comment));
                    f.el->add_child(*created);
                }
            } else if (std::string_view cdata; parse_cdata(cdata)) {
                add_text(cdata, comment_opening, *f.el);
            } else {
                syntax_error();
            }

            break;
        }

        case '?':
            if (!parse_pi())
                syntax_error();

            break;

        default: {
            const char* const child_opening = at_;
            std::size_t child_scope_mark = 0;
            node* const child = parse_start_tag(child_scope_mark);

            if (child == nullptr)
                syntax_error();

            f.el->add_child(*child);

            const bool nested = has_content();

            // Set before the push, which is what invalidates f.
            continue_text(f);

            if (nested)
                push(*child, child_opening, child_scope_mark);
            else
                scopes_.resize(child_scope_mark);  // an empty element declares only for itself

            continue;
        }
        }

        continue_text(f);
    }

    return root;
}

/// `document ::= XMLDecl? Misc* doctypedecl? Misc* element Misc*`
node* parser::parse_document() {
    parse_xml_declaration();                                  // optional
    parse_miscs();

    parse_xml_doctype();                                      // optional
    parse_miscs();

    node* const el = parse_element();

    if (el == nullptr)
        syntax_error();

    parse_miscs();

    // Without this a second root -- or any text at all -- would end the parse
    // where it stands and be dropped in silence.
    if (peek() != '\0')
        syntax_error();

    return el;
}

}  // namespace wxl::xml
