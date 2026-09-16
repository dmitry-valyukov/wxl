
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
//  - a wxl::core::sta_memory_pool has to exist before the first parse and
//    outlive the last object of this library, since that is where all of its
//    bookkeeping comes from. A pool may be built once per process, so it
//    belongs in main(), or in a global fixture in a test binary.
//  - a document may nest as deeply as it likes: neither the parse nor
//    node::find() recurses. Code of one's own that walks children by calling
//    itself does have a depth at which it dies, and nothing here can help it.

export module wxl.xml;

export import :arena;
export import :core;
export import :exception;
export import :node;
export import :encoding;
export import :parser;
export import :document;
