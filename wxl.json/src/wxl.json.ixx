// wxl::json -- a JSON reader whose tree consists of views and nothing else.
//
// It is modeled on wxl.xml: the same sequence of work (the buffer passes to
// the reader, is validated whole, and the tree then views it), the same arena
// for the tree, the same entry points -- load(), load_file(), root().
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

export module wxl.json;

// The public types return wxl::core::u8_view; the module re-exports wxl.core so
// that a caller can name that type without a second import.
export import wxl.core;

export import :exception;
export import :value;
export import :document;
