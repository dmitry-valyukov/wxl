// The tree builder every markup of the family drives: the element stack with
// its auto-closing and browser-style recovery, the whitespace rules with the
// pending space, the <pre> mode, the arena copies with their terminating
// zero, the diagnostic records. Extracted from the HTML parser when wxl.bb
// and wxl.rsdn appeared: a tokenizer knows what a tag looks like in its
// markup, and everything that happens *after* a tag is recognized is the
// same for all of them -- carefully debugged once, here.

export module wxl.html:builder;

import std;
import :node;
import wxl.core;
import wxl.xml;  // the arena -- wxl.xml's storage scheme, used as is

export namespace wxl::html {

/// Builds the document tree from tokenizer calls.
///
/// The tokenizer says "text", "line break", "open this tag with these
/// attributes", "close this tag" -- and the builder does the rest: blocks
/// auto-close inlines, `</p>` is optional, a mismatched close reopens the
/// formatting elements above it, whitespace collapses and dies at block
/// edges, `<pre>` keeps spaces and turns newlines into `<br>` nodes so every
/// text piece stays one whole terminated arena string.
///
/// Owned by document_builder (parse.ixx); a tokenizer only borrows it.
class tree_builder {
public:
    /// One builder per document; the arena and the error list are the
    /// document's own, handed in by document_builder.
    tree_builder(xml::arena& arena, xml::sta_vector<parse_error>& errors);

    tree_builder(const tree_builder&) = delete;
    tree_builder& operator=(const tree_builder&) = delete;

    // ---- text ----

    /// One character of content, under the whitespace rules: runs of
    /// whitespace collapse to one pending space, block edges trim, `<pre>`
    /// keeps everything and breaks lines into `<br>` nodes.
    void character(wchar_t c);

    /// An explicit line break (`<br>`, BB's newline): the space that would
    /// end the line dies, the new line starts fresh.
    void line_break();

    // ---- elements ----

    /// Opens an element; void tags (br, img) land without joining the
    /// stack. Attribute spans must live in the document's arena -- see
    /// copy_attributes().
    void open(tag_t tag, attribute_span attributes = {});

    /// Closes the topmost matching element, recovering browser-fashion:
    /// everything above the match closes with it, formatting elements among
    /// it reopen. A close with nothing to match is recorded and ignored.
    void close(tag_t tag);

    /// Whether the innermost open block is preformatted -- what a tokenizer
    /// asks before deciding how literally to treat its input.
    inline bool in_preformatted() const noexcept { return pre_depth_ > 0; }

    // ---- the arena ----

    /// Copies scratch text into the arena -- with the zero after it, which
    /// is the terminated-view promise of the whole tree.
    std::wstring_view copy(const xml::sta_wstring& text);

    /// The attributes of an element, copied into the arena for open().
    attribute_span copy_attributes(std::span<const attribute_t> attributes);

    // ---- diagnostics ----

    /// A recovery record; `detail` must live at least as long as the
    /// document -- a literal (canonical_name), or an arena copy.
    void record(error_t code, std::wstring_view detail);

    /// The same, with the detail copied into the arena first.
    void record_copied(error_t code, const xml::sta_wstring& detail);

    /// Flushes what is pending and answers the root. The builder is done;
    /// only document_builder calls this.
    node& finish();

private:
    void flush_text();
    void materialize_pending();
    void block_boundary();
    void close_top();
    void close_open_list_item();
    void open_in_table(tag_t tag, attribute_span attributes);
    void foster_cell();
    void pre_char(wchar_t c);
    node* append_new(tag_t tag, attribute_span attributes);

    static constexpr std::size_t kMaxErrors = 64;

    xml::arena& arena_;
    xml::sta_vector<parse_error>& errors_;

    node* root_;
    xml::sta_vector<node*> open_;

    xml::sta_wstring text_;
    bool pending_space_ = false;
    bool has_content_ = false;

    int pre_depth_ = 0;
    bool pre_fresh_ = false;
    bool last_was_cr_ = false;
};

}  // namespace wxl::html
