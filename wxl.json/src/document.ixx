// The document object: it takes the buffer, parses it, and owns the tree and
// everything the tree refers to. The library overview is in its primary
// interface, wxl.json.ixx.

export module wxl.json:document;

import :value;
import std;
import wxl.core;
import wxl.xml;  // the arena: the storage scheme of wxl.xml, used as is

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
