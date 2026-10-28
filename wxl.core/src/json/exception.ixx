// The exceptions the reader throws, and nothing else. The partition depends only
// on the standard library, not on the tree or the parser.

export module wxl.core:json.exception;

import std;

export namespace wxl::json {

/// Serves as the base class of every exception this reader throws: for a file that
/// cannot be read, bytes that are not UTF-8, a document that does not parse. One base
/// type lets `catch (const wxl::json::exception&)` catch any failure of this library
/// and no other.
///
/// The message is UTF-8: `std::exception` stores it, and the rest of the library
/// returns text in the same encoding.
class exception : public std::runtime_error {
public:
    /// Creates an exception with the message `message`. The message is copied; `what()`
    /// returns it.
    inline explicit exception(std::string_view message)
        : std::runtime_error(std::string(message)) {}
};

/// Represents the exception thrown when a document does not parse; carries the position
/// of the error.
class parsing_exception : public exception {
public:
    /// Creates a parsing exception for the position `line`, `column` in the document `file_name`.
    ///
    /// The message returned by `what()` writes the position the way a compiler does:
    /// `file_name(line,column): reason`, or `(line,column): reason` when `file_name` is
    /// empty. `rule` is the location in the reader's source code where the document was
    /// rejected.
    parsing_exception(std::string_view file_name, int line, int column, std::string_view reason,
                      const std::source_location& rule);

    /// Gets the document name and the position in it, one-based; the column counts bytes.
    ///
    /// Where a rule abandons a construct it has begun -- a string without the closing
    /// quote, an object without the closing brace -- the position is its beginning, not
    /// the byte where parsing stopped, because that is where the error is.
    /// @{
    inline std::string_view file_name() const noexcept { return file_name_; }
    /// Returns the line number, counted from one.
    inline int line() const noexcept { return line_; }
    /// Returns the column number in bytes, counted from one.
    inline int column() const noexcept { return column_; }
    /// @}

    /// Returns what is wrong with the document in one line, such as `expected ',' or '}'`.
    /// The position tells where to look; the reason tells what is wrong there.
    inline std::string_view reason() const noexcept { return reason_; }

    /// Returns the location in the reader's own source code where the document was
    /// rejected, that is, the grammar rule that failed on it. It is meant for debugging
    /// the reader, not the document, and so accompanies the position rather than
    /// replacing it.
    inline const std::source_location& rule() const noexcept { return rule_; }

private:
    // Copies, not views: the exception can outlive the parse that threw it, and the
    // file name has to outlive it as well. Ordinary strings, not pool-allocated: they
    // are filled once, and an object that travels this far from the reader should
    // hold no memory from the STA pool.
    std::string file_name_;
    std::string reason_;

    std::source_location rule_;

    int line_;
    int column_;
};

}  // namespace wxl::json
