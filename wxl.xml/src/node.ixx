
// The tree a parse produces: nodes, their attributes, and the little that can
// be asked of them. Everything here is a view into the document the reader
// owns, so all of it lives exactly as long as that document does.
//
// A view of checked text, at that. The whole document is validated once before
// the grammar walks it, so every name, every attribute value and every piece of
// text cut out of it is well-formed UTF-8 by construction -- and answering with
// wxl::core::u8_view is how that reaches the caller, who would otherwise have
// to say assume_valid() over and over to repeat what the reader already knows.
// Comparing one to an ordinary literal reads exactly as it did before.
//
// That type is what the tree holds, and not only what it answers with. The
// guarantee is established once, where the parser cuts a piece out of a buffer
// it has already checked, and travels with the piece from there; a node that
// stored plain bytes and put the guarantee back on at every read would be
// making the same claim over and over in the place least able to check it. It
// costs nothing to carry: a checked view is a view and a char8_t is a char.

export module wxl.xml:node;

import :core;
import wxl.core;

export namespace wxl::xml {

class node;

/// The children of a node: the nodes themselves, linked through the field they
/// carry for it. Nothing is allocated to hold a child, and nothing but the
/// arena holds one.
using node_list = core::intrusive_slist<node>;

/// What a node is. One concrete type carries all of them -- with children on
/// an intrusive list and attributes on a span, a node that has neither carries
/// two null pointers and a zero, and a class hierarchy would only add a vptr
/// to save them -- so this is what tells them apart.
enum class node_type : std::uint8_t {
    element,  ///< `<p …>`: has a name, attributes and children
    text,     ///< the text between markup, references resolved and CDATA folded in
    comment,  ///< `<!-- … -->`, kept only when the reader was asked to
};

/// A name as XML writes it: `href`, `xlink:href`, `p`. Both a node and an
/// attribute have one, and both answer the same three questions about it.
///
/// The parts are not stored apart: `qualified` is one view into the document,
/// and the colon inside it says where the prefix ends. A namespace URI comes
/// from the declaration in scope -- also a view, into the same document.
class qualified_name {
public:
    constexpr qualified_name() noexcept = default;

    constexpr qualified_name(core::u8_view qualified, std::size_t prefix_length,
                             core::u8_view namespace_uri) noexcept
        : qualified_(qualified), namespace_uri_(namespace_uri),
          prefix_length_(static_cast<std::uint32_t>(prefix_length)) {}

    /// The local part -- what the name is without saying whose it is. This is
    /// what code compares against, since the prefix a document happens to use
    /// is the document's business: `xlink:href` and `xl:href` are the same
    /// attribute, and both answer "href" here.
    ///
    /// Cut rather than stored: the colon inside the qualified name says where
    /// the prefix ends, and a name XML wrote is cut at a colon, which is ASCII
    /// and so never falls inside a UTF-8 sequence.
    inline core::u8_view name() const noexcept {
        return qualified_.substr(prefix_length_ ? prefix_length_ + 1 : 0);
    }

    /// The prefix, empty when the name carries none.
    inline core::u8_view prefix() const noexcept { return qualified_.substr(0, prefix_length_); }

    /// The namespace the prefix stands for -- or, for an unprefixed element,
    /// the default namespace in scope. Empty when there is none: an unprefixed
    /// attribute is in no namespace, whatever the default is, which is what
    /// the Namespaces recommendation says.
    inline core::u8_view namespace_uri() const noexcept { return namespace_uri_; }

    /// The name as the document wrote it, prefix included.
    inline core::u8_view qualified() const noexcept { return qualified_; }

    /// Both parts, for code that means a particular attribute of a particular
    /// vocabulary rather than whatever the local name happens to match.
    inline bool is(std::string_view uri, std::string_view local) const noexcept {
        return namespace_uri_ == uri && name() == local;
    }

private:
    core::u8_view qualified_;
    core::u8_view namespace_uri_;
    std::uint32_t prefix_length_ = 0;
};

/// One attribute of an element.
class attribute {
public:
    constexpr attribute(qualified_name name, core::u8_view value) noexcept : name_(name), value_(value) {}

    inline core::u8_view name() const noexcept { return name_.name(); }
    inline core::u8_view prefix() const noexcept { return name_.prefix(); }
    inline core::u8_view namespace_uri() const noexcept { return name_.namespace_uri(); }
    inline core::u8_view qualified_name() const noexcept { return name_.qualified(); }
    constexpr const xml::qualified_name& full_name() const noexcept { return name_; }

    constexpr core::u8_view value() const noexcept { return value_; }

private:
    xml::qualified_name name_;
    core::u8_view value_;
};

/// The attributes of an element, in document order: one array in the arena.
using attribute_span = std::span<const attribute>;

/// A node of the document tree.
///
/// A node says where in the document it is, but not which document: that name
/// belongs to the document object, and repeating it in every one of a
/// document's tens of thousands of nodes would buy nothing.
///
/// Neither copyable nor movable, and not destroyed either: nodes are built in
/// the reader's arena and released with it.
class node : public core::intrusive_slist_node<node> {
public:
    inline node(node_type type, qualified_name name, int line, int column) noexcept
        : name_(name), type_(type), line_(line), column_(column) {}

    inline node_type type() const noexcept { return type_; }
    inline bool is_element() const noexcept { return type_ == node_type::element; }
    inline bool is_text() const noexcept { return type_ == node_type::text; }

    /// The local name of an element; empty for anything else.
    inline core::u8_view name() const noexcept { return name_.name(); }
    inline core::u8_view prefix() const noexcept { return name_.prefix(); }
    inline core::u8_view namespace_uri() const noexcept { return name_.namespace_uri(); }
    inline core::u8_view qualified_name() const noexcept { return name_.qualified(); }
    inline const xml::qualified_name& full_name() const noexcept { return name_; }

    /// What a text node or a comment holds. An element answers empty: its text
    /// stands in the text nodes under it, which text_pieces() walks.
    constexpr core::u8_view value() const noexcept { return value_; }

    /// Line and column of the '<' this node starts at, 1-based; for a text
    /// node, of its first byte. The column counts bytes, not characters, which
    /// is what a text editor showing a UTF-8 file counts too.
    inline int line() const noexcept { return line_; }
    inline int column() const noexcept { return column_; }

    inline const node* parent() const noexcept { return parent_; }

    /// The children, in document order.
    inline const node_list& children() const noexcept { return children_; }

    /// The index-th element child of that local name, or nullptr when there is
    /// no such child.
    const node* child(std::string_view name, std::size_t index = 0) const noexcept;

    /// Same as child(): `node("Entry", 2)` is the third `Entry` child.
    inline const node* operator()(std::string_view name, std::size_t index = 0) const noexcept {
        return child(name, index);
    }

    /// The element children of that local name, for a range-for:
    /// `for (const node& row : table.children_named("tr"))`.
    class named_range;
    named_range children_named(std::string_view name) const noexcept;

    /// The pieces of text under this node, in document order, as views into
    /// the document. A node of text is one piece, its own; an element is as
    /// many as the markup inside it leaves, and most often exactly one.
    ///
    /// Nothing here joins them, and that is the point: the caller is what
    /// knows which string type the answer has to have and how long it has to
    /// live, and what it usually finds is a single piece already lying whole
    /// in the document, with nothing to join at all.
    class text_range;
    text_range text_pieces() const noexcept;

    inline attribute_span attributes() const noexcept { return attributes_; }

    /// The value of the attribute with that local name, whatever namespace it
    /// is in. Enough wherever a document has one vocabulary, which is most of
    /// them; use the two-argument form to mean one particular attribute.
    std::optional<core::u8_view> attribute(std::string_view name) const noexcept;
    std::optional<core::u8_view> attribute(std::string_view namespace_uri,
                                           std::string_view name) const noexcept;

    inline bool has_attribute(std::string_view name) const noexcept {
        return attribute(name).has_value();
    }

    /// The value of an attribute that has to be there.
    ///
    /// @throw exception when it is not. Its message names the element and the
    /// place, which is everything a node knows; a caller wanting the document
    /// named too adds document::file_name() where it catches this.
    core::u8_view required_attribute(std::string_view name) const;

    /// Depth-first search for the first element of that local name, this node
    /// included. Walked by the links a node already carries, so that a deep
    /// tree is walked rather than survived and nothing is allocated on the way.
    ///
    /// @return nullptr when the subtree holds no such element.
    const node* find(std::string_view name) const noexcept;

private:
    // A tree is assembled by the reader and read by everyone else.
    friend class parser;

    /// The next node of the subtree under `root`, in document order, or null
    /// once the walk has come back to `root`. Down to the first child, and
    /// where a level runs out up the parents until one has a sibling left:
    /// the links a node already carries are the whole of what a walk of any
    /// depth needs to remember.
    static const node* next_in_subtree(const node* at, const node* root) noexcept;

    inline void set_value(core::u8_view text) noexcept { value_ = text; }
    inline void set_attributes(const attribute_span list) noexcept { attributes_ = list; }

    inline void add_child(node& child) {
        child.parent_ = this;
        children_.push_back(core::as_not_null(&child));
    }

    xml::qualified_name name_;
    core::u8_view value_;

    attribute_span attributes_;
    node_list children_;
    node* parent_ = nullptr;

    node_type type_;
    int line_;
    int column_;
};

/// The element children of one name. Built by node::children_named().
class node::named_range {
public:
    class iterator {
    public:
        iterator() noexcept = default;
        inline iterator(node_list::const_iterator at, node_list::const_iterator end,
                        std::string_view name) noexcept
            : at_(at), end_(end), name_(name) {
            skip_to_match();
        }

        inline const node& operator*() const noexcept { return *at_; }
        inline const node* operator->() const noexcept { return &*at_; }

        inline iterator& operator++() noexcept {
            ++at_;
            skip_to_match();
            return *this;
        }

        inline bool operator==(const iterator& other) const noexcept { return at_ == other.at_; }

    private:
        inline void skip_to_match() noexcept {
            while (at_ != end_ && !(at_->is_element() && at_->name() == name_))
                ++at_;
        }

        node_list::const_iterator at_{};
        node_list::const_iterator end_{};
        std::string_view name_;
    };

    inline named_range(const node_list& children, std::string_view name) noexcept
        : children_(&children), name_(name) {}

    inline iterator begin() const noexcept {
        return iterator(children_->begin(), children_->end(), name_);
    }
    inline iterator end() const noexcept {
        return iterator(children_->end(), children_->end(), name_);
    }

private:
    const node_list* children_;
    std::string_view name_;
};

inline node::named_range node::children_named(const std::string_view name) const noexcept {
    return named_range(children_, name);
}

/// The pieces of text under one node. Built by node::text_pieces().
class node::text_range {
public:
    class iterator {
    public:
        using value_type = core::u8_view;
        using difference_type = std::ptrdiff_t;
        using iterator_concept = std::forward_iterator_tag;

        iterator() noexcept = default;
        inline iterator(const node* root, const node* at) noexcept : root_(root), at_(at) {
            skip_to_text();
        }

        inline core::u8_view operator*() const noexcept { return at_->value(); }

        inline iterator& operator++() noexcept {
            at_ = node::next_in_subtree(at_, root_);
            skip_to_text();
            return *this;
        }

        inline iterator operator++(int) noexcept {
            const iterator before = *this;
            ++*this;
            return before;
        }

        inline bool operator==(const iterator& other) const noexcept { return at_ == other.at_; }

    private:
        // A comment is markup about the document rather than text of it, and
        // an element keeps its text in the nodes below it -- so the walk goes
        // past both instead of reporting them.
        inline void skip_to_text() noexcept {
            while (at_ && !at_->is_text())
                at_ = node::next_in_subtree(at_, root_);
        }

        const node* root_ = nullptr;
        const node* at_ = nullptr;
    };

    inline explicit text_range(const node& root) noexcept : root_(&root) {}

    inline iterator begin() const noexcept { return iterator(root_, root_); }
    inline iterator end() const noexcept { return iterator(root_, nullptr); }

private:
    const node* root_;
};

inline node::text_range node::text_pieces() const noexcept {
    return text_range(*this);
}

}  // namespace wxl::xml
