// wxl::xml -- an XML reader whose tree is views and nothing else.
//
// The reader takes the document into its own hands and keeps it: every name
// and every piece of text a node holds is a view into that buffer, so nothing
// is copied on the way into the tree. Only a value the document interrupts --
// one holding a character reference, which stands for bytes appearing nowhere
// in it -- is assembled in the arena instead.
//
// A view of checked text, wxl::core::u8_view: the document is validated whole
// before the grammar walks it, so a piece of it is well-formed UTF-8 by
// construction, and the tree says so rather than leaving every reader to
// wonder. Comparing one to an ordinary literal reads as it always did.
//
// Bytes, not characters: the reader parses UTF-8 and answers in UTF-8. Every
// character XML's grammar names is ASCII, and in UTF-8 an ASCII byte never
// occurs inside a multi-byte sequence, so a byte-oriented scanner reads a
// document of any language without decoding it.
//
// What a caller has to keep to, in full:
//
//  - a tree lives as long as the document object that produced it. Nothing
//    else it was read from has to outlive it -- not the caller's buffer, not
//    the file.
//  - the library takes all of its auxiliary storage from wxl::core::sta_memory_pool,
//    whose one static instance wxl.core builds before the program's own objects.
//    The pool serves only the thread that built it, so the library has to be
//    used on that thread.
//  - a document may nest as deeply as it likes: neither the parse nor
//    node::find() recurses. Code of one's own that walks children by calling
//    itself does have a depth at which it dies, and nothing here can help it.
//
// This partition is the document object: what takes a buffer over, parses it,
// and owns the tree and everything that tree views.

export module wxl.core:xml.document;

import :xml.arena;
import :xml.exception;
import :xml.node;
import :xml.parser;
import :unicode;
import std;

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
    /// Constructs a document that holds no tree yet and parses with the options \p opts.
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
    /// it got to -- a read aborted midway would otherwise be reported as a
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
