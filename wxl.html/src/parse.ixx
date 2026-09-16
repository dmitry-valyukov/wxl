// The door into the parser: parse() and the document that owns what it
// built -- plus the family door, document_builder, through which the
// sibling markups (wxl.bb, wxl.rsdn) build the same tree with the same
// recovery. The parsing itself is parse.cpp.

export module wxl.html:parse;

import std;
import :builder;
import :node;
import wxl.core;
import wxl.xml;

export namespace wxl::html {

class document_builder;

/// A parsed tree and the storage it is carved from. Self-contained: every
/// view in the tree points into the document's own arena, so the input the
/// tree was parsed from may die the moment parse() returns.
class document {
public:
    document(document&& other) noexcept;
    document& operator=(document&& other) noexcept;
    ~document();

    /// The synthetic node above the top-level content. Its children are the
    /// parsed blocks -- and, for markup that never opened one, bare inline
    /// nodes: wrapping those into the implicit paragraph is the builder's
    /// business, not the tree's.
    const node& root() const noexcept;

    /// The recoveries, in input order. Capped at a few dozen: hostile input
    /// repeats its garbage, and diagnostics need the shape of the problem,
    /// not every instance of it.
    std::span<const parse_error> errors() const noexcept;

private:
    struct state;

    explicit document(state* state) noexcept : state_(state) {}

    state* state_;

    friend class document_builder;
};

/// The construction site of a document, for the parsers of the family:
/// wxl.html itself and its siblings (wxl.bb, wxl.rsdn). A tokenizer drives
/// tree() and takes the finished document; the arena, the error list and
/// the recovery rules come with it.
///
class document_builder {
public:
    document_builder();
    ~document_builder();

    document_builder(const document_builder&) = delete;
    document_builder& operator=(const document_builder&) = delete;

    tree_builder& tree() noexcept { return *tree_; }

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
document parse(std::wstring_view input);

/// The same, for checked UTF-8 -- the second explicit door the spec names:
/// text that arrived from outside and was verified up top. Transcoded once
/// and parsed as the wide overload.
document parse(core::u8_view input);

/// The tree back as canonical markup: synonyms folded, whitespace already
/// collapsed, recovery already done. The golden-test format of the whole
/// family, and a debugging eye into any parsed tree.
std::wstring serialized(const node& root);

}  // namespace wxl::html
