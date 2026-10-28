// wxl::json -- a JSON reader whose tree consists of views and nothing else.
//
// It is modeled on the XML reader: the same sequence of work (the buffer
// passes to the reader, is validated whole, and the tree then views it), the
// same arena for the tree, the same entry points -- load(), load_file(), root().
//
// Checked text, wxl::core::u8_view: the document is validated whole before the
// grammar walks it, so any piece of it is well-formed UTF-8. The tree stores
// that type rather than converting to it on return; otherwise every caller
// would write assume_valid() and repeat what the reader already knows.
//
// Bytes, not characters. All of the JSON grammar is ASCII, and in UTF-8 an
// ASCII byte never occurs inside a multi-byte sequence, so a byte-oriented
// scanner reads a document in any language without decoding it. Characters
// appear only in the \uXXXX escape: it names a UTF-16 code unit that is not
// present in the document as bytes, so such a string is assembled in the arena.
//
// Comments are always accepted, and there is no switch for them: wxl's own
// configuration files (the profiles `wxl.gen/profiles/*.winxl-profile` and the
// tables beside them) carry comments that explain the settings next to them.
// Everything else follows RFC 8259 strictly: no trailing comma, no unquoted
// member name, no leading zero in a number, because misreading a document costs
// more than rejecting it.
//
// What a caller has to keep to:
//
//  - a tree lives exactly as long as the document object that built it.
//    Nothing else has to outlive the tree, neither the caller's buffer nor the
//    file;
//  - the library takes all of its auxiliary memory from wxl::core::sta_memory_pool,
//    whose one static instance wxl.core builds before the program's own objects.
//    The pool serves only the thread that built it, so the library has to be
//    used on that thread;
//  - nesting depth is not limited: the parse is not recursive and uses an
//    explicit stack. A traversal of the tree that calls itself does have a
//    depth limit, and this library cannot remove it.
//
// This partition is the document object: it takes the buffer, parses it, and
// owns the tree and everything the tree refers to.

export module wxl.core:json.document;

import :json.value;
import :xml.arena;  // the arena: the storage scheme of the XML reader, used as is
import std;

export namespace wxl::json {

/// Represents a parsed document that owns its text: the tree and everything it refers to.
///
/// The parser is not part of it: the parser lives on the stack of the load that needs
/// it, and only its result stays here. The fields below are therefore the whole
/// document; no implementation object is hidden behind a pointer.
class document {
public:
    document() = default;

    document(const document&) = delete;
    document& operator=(const document&) = delete;

    /// Parses UTF-8 text, which the document takes ownership of and the tree then views.
    ///
    /// The buffer is taken rather than borrowed: the document keeps it, so the views of
    /// the tree do not depend on the caller's buffer, and the move copies nothing.
    ///
    /// \throw exception
    /// \throw parsing_exception
    const value& load(std::string&& source, std::string_view file_name = {});

    /// Reads the stream to its end and parses it as UTF-8.
    ///
    /// \throw exception when the stream fails before its end, with the offset of the
    /// byte reached; otherwise a truncated document would be reported as malformed, and
    /// the error would be looked for in the wrong place.
    /// \throw parsing_exception
    const value& load(std::istream& source, std::string_view file_name = {});

    /// Reads and parses a UTF-8 file; its path becomes the value of `file_name()`.
    ///
    /// \throw exception
    /// \throw parsing_exception
    const value& load_file(const std::filesystem::path& path);

    /// Returns the tree of the last load, or `nullptr` before the first one.
    inline const value* root() const noexcept { return root_; }

    /// Returns the file name given to the last load, or an empty string if none was
    /// given. Error messages take the file name from here; a position in the document
    /// carries only the line and the column.
    inline std::string_view file_name() const noexcept { return file_name_; }

private:
    /// Parses the current contents of `document_`.
    const value& parse_source();

    /// The owned text of the document. Names and strings without escape sequences are
    /// views into this buffer, which makes the tree independent of where the text was
    /// read from without copying it.
    std::string document_;

    /// The name of the document the tree comes from, kept here rather than in every value.
    std::string file_name_;

    /// Storage for the tree and for what the document does not contain as is: strings
    /// assembled from escape sequences. There is no arena before the first load, and
    /// each load creates its own: an arena is created when there is something to put in
    /// it and is destroyed with its tree, so it is never empty and never reused.
    std::unique_ptr<xml::arena> arena_;

    const value* root_ = nullptr;
};

}  // namespace wxl::json
