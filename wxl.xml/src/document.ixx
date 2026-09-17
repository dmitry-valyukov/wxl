// The document object: what takes a buffer over, parses it, and owns the tree
// and everything that tree views. The library's own introduction is on the
// primary interface, wxl.xml.ixx.

export module wxl.xml:document;

import :arena;
import :core;
import :exception;
import :node;
import :parser;
import wxl.core;

export namespace wxl::xml {

/// The whole of a file, in one allocation and one read: the shape a document
/// has to be in for document::load() to take it over.
///
/// Public because a caller reading its own documents should not have to get
/// this right a second time. An ordinary std::string, deliberately: the STA
/// pool this library otherwise allocates from is built for many small objects,
/// and a buffer whose size a document decides is not one of them.
///
/// @throw exception when the file cannot be read.
std::string read_file(const std::filesystem::path& path);

/// A document, read and owned: the tree and everything it views.
///
/// The parse itself is not here: it is `parser`, built on the stack of the
/// load that needs it, and what it leaves behind is what this holds. So the
/// fields below are the whole of a document -- there is no second object
/// hiding behind a pointer, because there is nothing left to hide.
class document {
public:
    inline explicit document(const options& opts = {}) : options_(opts) {}

    document(const document&) = delete;
    document& operator=(const document&) = delete;

    /// Parses UTF-8 the document takes over, and the tree then views.
    ///
    /// The buffer is taken rather than viewed because the reader needs it
    /// terminated: a std::string always holds a trailing zero, no document may
    /// contain one, and that is the sentinel every scan inside stops at
    /// instead of testing where the buffer ends.
    ///
    /// @throw exception, parsing_exception
    const node& load(std::string&& source, std::string_view file_name = {});

    /// Reads the stream to its end and parses it as UTF-8.
    ///
    /// @throw exception when the stream fails before it ends, naming the byte
    /// it got to -- a document cut short would otherwise be reported as a
    /// broken one, which sends its reader looking in the wrong place.
    /// @throw parsing_exception
    const node& load(std::istream& source, std::string_view file_name = {});

    /// Reads a UTF-8 file; its path is what file_name() then answers with.
    ///
    /// @throw exception, parsing_exception
    const node& load_file(const std::filesystem::path& path);

    /// The tree of the last load, or nullptr before the first one.
    inline const node* root() const noexcept { return root_element_; }

    /// The file the last load was given, empty when it was not named. This is
    /// where a diagnostic about a node gets the file to name: the node itself
    /// carries only the line and the column.
    inline std::string_view file_name() const noexcept { return file_name_; }

    /// The version the XML declaration announced; "1.0" when it did not.
    inline core::u8_view xml_version() const noexcept { return xml_version_; }

private:
    /// Parses whatever document_ now holds.
    const node& parse_source();

    /// The document, owned. Everything the tree hands out is a view into this
    /// buffer, which is what leaves the tree independent of where it was read
    /// from without a single byte being copied.
    std::string document_;

    /// The document the tree came from, kept here rather than in every node.
    std::string file_name_;

    const options options_;

    /// Where the tree lives, and everything it points at that the document
    /// itself does not hold. Nothing before the first load, and a new one for
    /// every load after that: an arena is built when there is a tree to put in
    /// it and dies with that tree, so it is never emptied and never reused.
    std::unique_ptr<arena> arena_;

    node* root_element_ = nullptr;

    core::u8_view xml_version_ = u8"1.0";
};

}  // namespace wxl::xml
