// wxl::html -- the HTML subset behind HtmlBlock.
//
// A tolerant parser for foreign, often broken markup -- forum posts, chat
// messages -- producing a lightweight DOM for one walk. The contract, in
// full, is docs/html.md; the two rules everything here serves:
//
//  - the text is sacred, the tags are not: no input ever throws, an unknown
//    tag disappears and leaves its content standing, a mismatched close
//    closes and reopens the way browsers do;
//  - every parse is complete in itself: the tree closes all its blocks, and
//    neither the source nor any parser state survives the call -- content
//    may arrive in independent chunks.
//
// A wxl::core::sta_memory_pool has to exist before the first parse and
// outlive the last document: the tree is carved from an arena over that
// pool, the storage scheme the XML reader proved out.
//
// This partition is the door into the parser: parse() and the document that
// owns what it built -- plus the family door, document_builder, through which
// the sibling markups (wxl.bb, wxl.rsdn) build the same tree with the same
// recovery. The parsing itself is parse.cpp.

export module wxl.core:html.parse;

import std;
import :html.builder;
import :html.node;
import :unicode;

export namespace wxl::html {

class document_builder;

/// A parsed tree and the storage it is carved from. Self-contained: every
/// view in the tree points into the document's own arena, so the input the
/// tree was parsed from may die the moment parse() returns.
class document {
public:
    /// Constructs a document that takes over the tree and the storage of
    /// `other`. `other` is left empty: it can be destroyed or assigned to, and
    /// `root()` and `errors()` must not be called on it.
    document(document&& other) noexcept;
    /// Releases the tree and the storage of this document and takes over those
    /// of `other`, which is left empty. Self-assignment has no effect.
    document& operator=(document&& other) noexcept;
    ~document();

    /// The synthetic node above the top-level content. Its children are the
    /// parsed blocks -- and, for markup that never opened one, bare inline
    /// nodes: wrapping those into the implicit paragraph is the builder's
    /// business, not the tree's.
    const node& root() const noexcept;

    /// The recoveries, in input order. At most 64 are recorded and later ones
    /// are dropped: hostile input repeats its garbage, and diagnostics need
    /// the shape of the problem, not every instance of it.
    std::span<const parse_error> errors() const noexcept;

private:
    struct state;

    inline explicit document(state* state) noexcept : state_(state) {}

    state* state_;

    friend class document_builder;
};

/// The construction site of a document, for the parsers of the family:
/// this one itself and its siblings (wxl.bb, wxl.rsdn). A tokenizer drives
/// tree() and takes the finished document; the arena, the error list and
/// the recovery rules come with it.
///
class document_builder {
public:
    document_builder();
    ~document_builder();

    document_builder(const document_builder&) = delete;
    document_builder& operator=(const document_builder&) = delete;

    /// Gets the tree builder a tokenizer drives; valid until `finish()`.
    inline tree_builder& tree() noexcept { return *tree_; }

    /// The finished document; the builder is spent.
    document finish() && noexcept;

private:
    document::state* state_;
    tree_builder* tree_;
};

/// Parses the subset (design.md). Never throws on bad markup, whatever it
/// is: unknown tags vanish and leave their content, mismatched closes are
/// recovered browser-fashion, a lone '<' is a character. What can throw is
/// only allocation.
document parse(std::u16string_view input);

/// The same, for checked UTF-8 -- the second explicit door the spec names:
/// text that arrived from outside and was verified up top. Transcoded once
/// and parsed as the UTF-16 overload.
document parse(core::u8_view input);

/// The tree back as canonical markup: synonyms folded, whitespace already
/// collapsed, recovery already done. The golden-test format of the whole
/// family, and a debugging eye into any parsed tree.
std::u16string serialized(const node& root);

}  // namespace wxl::html
