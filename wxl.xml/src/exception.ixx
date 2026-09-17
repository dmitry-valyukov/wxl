
// What this reader throws, and nothing else. Its own partition because a
// caller that only catches -- a consumer wrapping the reader, a test -- needs
// these two types and neither the tree nor the parser.

export module wxl.xml:exception;

import :core;

export namespace wxl::xml {

/// The root of what this reader throws: a file it cannot read, bytes that are
/// not UTF-8, an attribute that has to be there and is not, a document that
/// does not parse. One type over all of them, so `catch (const
/// wxl::xml::exception&)` catches every failure of this library and no other.
///
/// The message is UTF-8, which is what std::exception takes and what
/// everything else here hands out.
class exception : public std::runtime_error {
public:
    inline explicit exception(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};

/// Thrown when a document does not parse; carries where the parser stopped.
class parsing_exception : public exception {
public:
    parsing_exception(std::string_view file_name, int line, int column, const std::source_location& rule);

    /// The document, and the place in it to look at -- 1-based, the column
    /// counted in bytes.
    ///
    /// Where a rule refuses something it had already begun -- an element left
    /// open, a comment never terminated -- that beginning is the place, rather
    /// than the byte the parser gave up on.
    /// @{
    inline std::string_view file_name() const noexcept { return file_name_; }
    inline int line() const noexcept { return line_; }
    inline int column() const noexcept { return column_; }
    /// @}

    /// Where in this reader's own sources the document was refused, i.e. which
    /// grammar rule gave up on it. For debugging the reader rather than the
    /// document, which is why it is beside the position above and not instead
    /// of it -- the two answer different questions.
    inline const std::source_location& rule() const noexcept { return rule_; }

private:
    // A copy rather than a view: an exception may well outlive the parser that
    // threw it, and the file name it carries has to survive with it. An
    // ordinary string, not a pooled one: it is filled once, and an object that
    // travels this far out of the reader is better off owing the STA pool
    // nothing.
    std::string file_name_;

    std::source_location rule_;

    int line_;
    int column_;
};

}  // namespace wxl::xml
