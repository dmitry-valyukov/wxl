// The grammar: one parse, and nothing that outlives it.
//
// Everything here is scratch -- where the reading stands, the elements whose
// content is half-read, the attributes of the element being read, the
// namespace declarations in scope. None of it means anything once the tree is
// built, so none of it belongs to the document: a parser is built on the stack
// of the load that needs it and is gone when that load returns. Two invariants
// that used to be kept by hand -- clearing five buffers before a parse and
// nulling four pointers after one -- are not expressible any more.
//
// It is exported for one reason: a rule is worth testing on its own, and a
// test lives outside. So the rules that stand alone are public, and a caller
// may drive them: point a parser at a fragment and ask it for a Name, a
// reference, a comment. The rules that only mean anything inside a running
// parse -- those that push elements, resolve prefixes or build nodes -- are
// private, and parse() is what runs them.
//
// `options` lives here rather than beside the document for the same reason:
// what it steers is the grammar. A document takes a set and passes it on, and
// that is the whole of the arrow between the two -- document knows about the
// parse, the parse knows nothing about documents.

export module wxl.xml:parser;

import :arena;
import :core;
import :exception;
import :node;
import wxl.core;

export namespace wxl::xml {

/// What a caller may steer about the reading. Given to the document once,
/// since it describes the reader rather than a file.
struct options {
    /// Whether the text between markup becomes text nodes. On, because that is
    /// the only way a tree can say where text stands among its siblings, and
    /// mixed content -- `<p>a<em>b</em>c</p>`, which is what a paragraph of a
    /// book is -- means nothing without it.
    ///
    /// Off, text is read and dropped: nothing here joins it into a value of
    /// its own, since a reader that copied the bytes between the tags would be
    /// charging for them whether or not anybody meant to look. What that buys
    /// is a node per run of text not built at all, which is what a document
    /// read for its markup wants -- 34 136 of them in WinUI3's generic.xaml.
    ///
    /// Whitespace is kept as it stands, all of it. Between `</p>` and `<p>` it
    /// is indentation and between text and `<em>` it is a space the reader
    /// will see, and nothing here can tell the two apart: that takes knowing
    /// what the elements mean, which belongs to whoever defined them.
    bool keep_text = true;

    /// Whether a comment becomes a node of its own instead of being read and
    /// dropped. Off, because what reads a document reads it for its markup: a
    /// comment kept is a node built, walked past, and tested against every
    /// child.
    ///
    /// Comments between top-level markup are dropped either way -- they belong
    /// to no element, and there is nowhere to put them.
    bool keep_comments = false;
};

class parser {
public:
    /// \param source the document, byte order mark already dropped and the
    ///        bytes already checked -- validating is the caller's, because it
    ///        is one pass over the whole buffer rather than a rule. It has to
    ///        end in a zero: that is the sentinel every scan here stops at,
    ///        which is why a document arrives as a std::string and a fragment
    ///        in a test arrives as a literal.
    /// \param into where the tree is built. Outlives the parser, being what
    ///        holds the tree afterwards.
    /// \param file_name what a parsing_exception names. A view, so it outlives
    ///        this too.
    parser(std::string_view source, arena& into, const options& opts = {},
           std::string_view file_name = {});

    parser(const parser&) = delete;
    parser& operator=(const parser&) = delete;

    /// Reads the whole document and answers with its root element.
    ///
    /// @throw parsing_exception
    node& parse();

    /// The version the XML declaration announced; "1.0" when it did not.
    core::u8_view xml_version() const noexcept { return xml_version_; }

    /// Where the parser stands, and how it moves. One pointer and no bound:
    /// the document is a std::string, which always keeps a terminating zero,
    /// and no document may hold one -- so every loop below stops there and none
    /// has to compare against the end of the buffer.
    /// @{

    /// The byte the parser stands on, or 0 at the end of the document.
    char peek() const noexcept { return *at_; }

    /// Steps over the byte peek() answered with.
    void advance() noexcept { ++at_; }

    /// Reads a byte and steps over it, staying where it is at the end.
    char next_char() noexcept {
        const char c = *at_;

        if (c != '\0')
            ++at_;

        return c;
    }

    /// Whether the document reads like this here, without moving.
    bool looks_like(std::string_view text) const noexcept;

    /// What is left, from where the parser stands to the end.
    std::string_view rest() const noexcept { return at_; }
    /// @}

    /// The rules that stand alone: each answers whether it matched, and leaves
    /// the parser after what it took or exactly where it found it. Those that
    /// are not noexcept may also decide the document is broken, having matched
    /// their beginning.
    /// @{
    bool parse_spaces() noexcept;
    bool parse_string(std::string_view text) noexcept;
    bool parse_string_no_case(std::string_view text) noexcept;
    bool parse_number(char32_t& number) noexcept;
    bool parse_hex_number(char32_t& number) noexcept;
    bool parse_char(char character) noexcept;
    bool parse_name(std::string_view& name) noexcept;
    bool parse_eq() noexcept;
    bool parse_version_num(std::string_view& version) noexcept;
    bool parse_enc_name() noexcept;

    /// A reference, `&amp;` or `&#x41;`, into the bytes it stands for. What a
    /// character reference stands for appears nowhere in the document, so that
    /// answer is built in the arena.
    std::string_view parse_reference_text();

    bool parse_attribute_value(std::string_view& value);
    bool parse_cdata(std::string_view& text);
    bool parse_comment(std::string_view& text);
    /// @}

private:
    /// A line and a column, 1-based, the column counted in bytes.
    struct line_column {
        int line;
        int column;
    };

    /// Where a position in the document falls. Counted when asked rather than
    /// while reading: only an element's start and a syntax error ever ask, and
    /// since they ask in document order, all of them together walk the
    /// newlines once.
    line_column place(const char* position) noexcept;

    /// The pieces one attribute value arrives in, collected without their
    /// bytes: each is a view, sixteen bytes whatever its length, and nothing
    /// is copied until it is known that copying is needed at all -- which is
    /// only where a reference interrupted the value.
    ///
    /// An attribute value is the one thing here that has to come out whole,
    /// since `attribute::value()` is a single view and what a reference stands
    /// for is nowhere in the document to point at. Text is not collected: it
    /// leaves as the pieces it arrives in. One buffer serves, because
    /// attributes are read in a loop that nothing can interrupt.
    /// @{
    void add_segment(std::string_view piece);
    std::string_view take_segments();
    /// @}

    /// One element whose content is being read: what a grammar that called
    /// itself would hold on the machine's stack instead.
    struct frame {
        node* el;                 ///< whose content this is
        const char* opening;      ///< the '<' its start tag begins at, for a diagnostic
        const char* from;         ///< where the piece of text being read begins
        std::size_t scope_mark;   ///< where the namespace scopes stood before this element
    };

    /// Text goes on from here: the next piece begins where the last one ended.
    void continue_text(frame& f) noexcept { f.from = at_; }

    /// One piece of text, as a node of its own, where the reader was asked to
    /// keep text at all.
    ///
    /// Nothing is joined. A run that a reference, a CDATA section or a comment
    /// interrupts comes out as the several pieces it is, each a view into the
    /// document, and putting them back together belongs to the caller -- who
    /// is the one that knows which string type the answer has to be and how
    /// long it has to live.
    void add_text(std::string_view piece, const char* position, node& parent);

    /// Starts reading the content of an element, whose start tag is over.
    void push(node& el, const char* opening, std::size_t scope_mark);

    /// The namespace a prefix stands for here, empty when nothing declared it.
    core::u8_view namespace_for(std::string_view prefix) const noexcept;

    /// A qualified name as the document wrote it, with its namespace looked up.
    /// An unprefixed attribute is in no namespace whatever the default is --
    /// that is what the Namespaces recommendation says, and the difference is
    /// why this takes the kind rather than guessing it.
    qualified_name resolve(std::string_view qualified, bool is_attribute) const noexcept;

    /// Pushes what the start tag just read declares, and answers where the
    /// scope stack stood before -- what the element's end tag cuts back to.
    std::size_t open_scope();

    /// A declaration's opening -- spaces, the keyword, the equal sign.
    bool parse_declaration_opening(std::string_view declaration);

    bool parse_xml_declaration();
    bool parse_xml_doctype();

    /// Runs past a quoted string and past an internal subset. Neither is read,
    /// but both have to be walked whole: either may hold the byte that would
    /// otherwise end the declaration around it.
    /// @{
    void skip_quoted(char quote);
    void skip_internal_subset();
    /// @}

    bool parse_version_info(std::string_view& version);
    bool parse_encoding_decl();
    bool parse_standalone_declaration();

    /// Comments, processing instructions and spaces, in any number.
    void parse_miscs();

    /// A processing instruction, read and dropped.
    bool parse_pi();

    bool parse_attribute();

    /// `opening` is where the element being closed began -- see syntax_error.
    void parse_etag(const node& el, const char* opening);

    /// What closes a start tag, and whether content follows it.
    bool has_content();

    node* parse_start_tag(std::size_t& scope_mark);
    node* parse_element();
    node* parse_document();

    /// Gives up on the document, naming a place in it and which rule here
    /// stopped -- the default argument is what makes every call site record
    /// itself without saying anything.
    ///
    /// The place is the byte the parser stands on, which is right whenever
    /// that byte is itself the problem. A rule refusing something it had
    /// already begun -- an element left open, a comment never terminated --
    /// passes that beginning instead, since it is what has to be fixed and may
    /// be any distance back.
    /// @{
    [[noreturn]] void syntax_error(
        const std::source_location& rule = std::source_location::current());
    [[noreturn]] void syntax_error(
        const char* position,
        const std::source_location& rule = std::source_location::current());
    /// @}

    /// What the parse was given, and what it hands the answer to.
    /// @{
    arena& arena_;
    const options options_;
    const std::string_view file_name_;
    /// @}

    const char* at_ = nullptr;      ///< Where the parser stands.
    const char* start_ = nullptr;   ///< The document, less its byte order mark.

    /// How far place() has counted, and what it counted to.
    /// @{
    const char* place_at_ = nullptr;
    const char* place_line_ = nullptr;
    int place_number_ = 1;
    /// @}

    /// The pieces of every value being read at once, one stack for all of them.
    std::vector<std::string_view> segments_;

    /// The elements whose content is half-read, outermost first.
    std::vector<frame> stack_;

    /// Where an element's attributes are collected before the arena takes a
    /// copy. One buffer for the whole parse: attributes are read in a loop
    /// nothing else can interrupt.
    std::vector<attribute> attributes_;

    /// Имя и значение атрибута до разрешения пространства имён: объявления
    /// вида xmlns действуют на сам элемент, поэтому имена нельзя разрешать,
    /// пока не прочитан весь начальный тег.
    struct raw_attribute {
        std::string_view name;
        std::string_view value;
    };

    std::vector<raw_attribute> raw_attributes_;

    /// Объявления пространств имён, действующие сейчас: стек, растущий с
    /// каждым элементом и обрезаемый на его закрывающем теге. Поиск идёт
    /// с конца — ближайшее объявление перекрывает дальнее.
    struct namespace_binding {
        std::string_view prefix;   ///< пусто для xmlns="..."
        std::string_view uri;
    };

    std::vector<namespace_binding> scopes_;

    /// What the XML declaration announced, for the document to take afterwards.
    core::u8_view xml_version_ = u8"1.0";

    /// A position in the document, to return to or to cut a view from.
    class bookmark {
    public:
        explicit bookmark(parser& reader) noexcept : parser_(reader), position_(reader.at_) {}

        /// Puts the parser back, so a rule that fails halfway leaves nothing
        /// behind.
        void restore() const noexcept { parser_.at_ = position_; }

        /// The text between the bookmark and the current position, less
        /// `end_skip` bytes of terminator the caller has already consumed.
        std::string_view substring(std::size_t end_skip = 0) const noexcept;

        /// Moves the bookmark to the current position.
        void reset() noexcept { position_ = parser_.at_; }

        /// Where in the document this bookmark points.
        line_column place() const noexcept { return parser_.place(position_); }

    private:
        parser& parser_;
        const char* position_;
    };
};

}  // namespace wxl::xml
