// The tree builder: the half of the old HTML parser that every markup of
// the family shares. The rules are the ones parse_test.cpp pins down; the
// HTML tokenizer (parse.cpp) and the BB/RSDN tokenizers only feed it.

module wxl.html;

import std;
import wxl.core;
import wxl.xml;

namespace wxl::html {

tree_builder::tree_builder(xml::arena& arena, xml::sta_vector<parse_error>& errors)
    : arena_(arena), errors_(errors) {
    root_ = arena_.create<node>(tag_t::root);
    open_.push_back(root_);
}

// ---- text, whitespace and the pending space ----
//
// Outside <pre>, runs of whitespace become one space, and that space is
// *pending* until real content follows in the same block: this is what
// trims block edges without ever un-writing anything -- a space that
// would end a block simply never gets written.

void tree_builder::character(wchar_t c) {
    if (pre_depth_ > 0) {
        pre_char(c);
        return;
    }
    if (c == L' ' || c == L'\t' || c == L'\r' || c == L'\n' || c == L'\f') {
        if (has_content_) pending_space_ = true;
        return;
    }
    foster_cell();
    if (pending_space_) {
        text_ += L' ';
        pending_space_ = false;
    }
    text_ += c;
    has_content_ = true;
}

void tree_builder::pre_char(wchar_t c) {
    // \r\n and a lone \r both become \n; the newline right after <pre>
    // is dropped, as HTML has it.
    if (c == L'\n' && last_was_cr_) {
        last_was_cr_ = false;
        return;
    }
    last_was_cr_ = c == L'\r';
    if (last_was_cr_) c = L'\n';
    if (pre_fresh_) {
        pre_fresh_ = false;
        if (c == L'\n') return;
    }
    if (c == L'\n') {
        // A line of its own plus an explicit break node -- the spec's
        // "\n becomes a LineBreak" done right here, so every text piece
        // the tree hands out is one whole arena string with its zero,
        // and no consumer ever has to cut one (a substring would lose
        // the terminator the tree promises).
        flush_text();
        node* br = arena_.create<node>(tag_t::br);
        open_.back()->append_child(*br);
        return;
    }
    text_ += c;
    has_content_ = true;
}

void tree_builder::line_break() {
    // The space that would end the line is dropped, and the new line
    // starts fresh -- same as a block edge, except the node sits inline.
    foster_cell();
    flush_text();
    pending_space_ = false;
    has_content_ = false;
    append_new(tag_t::br, {});
}

// The accumulated text as a node. Only what was materialized is written:
// a still-pending space stays pending across the flush.
void tree_builder::flush_text() {
    if (text_.empty()) return;
    node* piece = arena_.create<node>(copy(text_));
    open_.back()->append_child(*piece);
    text_.clear();
}

void tree_builder::materialize_pending() {
    if (pending_space_ && has_content_) text_ += L' ';
    pending_space_ = false;
}

// A block edge: whatever space was pending dies here, and content
// starts counting from zero on the other side.
void tree_builder::block_boundary() {
    flush_text();
    pending_space_ = false;
    has_content_ = false;
}

// Copies scratch text into the arena -- with the zero after it. One
// wchar_t buys the whole tree the param::hstring contract: every view
// the DOM hands out crosses into COM/WinRT as a string reference, no
// copy at the call. The scratch is always a basic_string, so reading
// its terminator at data()[size] is its own guarantee.
std::wstring_view tree_builder::copy(const xml::sta_wstring& text) {
    const std::span<const wchar_t> copied =
        arena_.copy(std::span<const wchar_t>(text.data(), text.size() + 1));
    return {copied.data(), copied.size() - 1};
}

attribute_span tree_builder::copy_attributes(std::span<const attribute_t> attributes) {
    return arena_.copy(attributes);
}

void tree_builder::record(error_t code, std::wstring_view detail) {
    // The cap keeps hostile input from growing the list without bound:
    // diagnostics need the shape of the problem, not every instance of it.
    if (errors_.size() < kMaxErrors) errors_.push_back({code, detail});
}

void tree_builder::record_copied(error_t code, const xml::sta_wstring& detail) {
    if (errors_.size() < kMaxErrors) errors_.push_back({code, copy(detail)});
}

// ---- the element stack ----

void tree_builder::close_top() {
    node* top = open_.back();
    if (is_block(top->tag())) {
        block_boundary();
        if (top->tag() == tag_t::pre) --pre_depth_;
    } else {
        flush_text();
    }
    open_.pop_back();
}

// Content where only rows and cells belong -- text straight inside
// <table> or <tr>, an inline, a nested table -- gets an implicit cell,
// and a row when even that is missing: the text is sacred, and a browser
// fosters it the same way. Rows and cells themselves never come here.
void tree_builder::foster_cell() {
    const tag_t top = open_.back()->tag();
    if (top != tag_t::table && top != tag_t::tr) return;
    block_boundary();
    if (top == tag_t::table) open_.push_back(append_new(tag_t::tr, {}));
    open_.push_back(append_new(tag_t::td, {}));
}

// A row or a cell: they live only inside a table, and each closes what
// its predecessor left open -- </td> and </tr> are as optional as </p>.
// Outside any table the tag is dropped and recorded; its content stays.
void tree_builder::open_in_table(tag_t tag, attribute_span attributes) {
    std::size_t table = 0;
    for (std::size_t i = open_.size(); i-- > 1;) {
        if (open_[i]->tag() == tag_t::table) {
            table = i;
            break;
        }
    }
    if (table == 0) {
        record(error_t::misplaced_tag, canonical_name(tag));
        return;
    }

    if (tag == tag_t::tr) {
        // A new row closes the open row and whatever the row held.
        while (open_.size() > table + 1) close_top();
    } else {
        // A cell closes the open cell; without a row it opens one.
        std::size_t row = 0;
        for (std::size_t i = open_.size(); i-- > table + 1;) {
            if (open_[i]->tag() == tag_t::tr) {
                row = i;
                break;
            }
        }
        if (row == 0) {
            while (open_.size() > table + 1) close_top();
            block_boundary();
            open_.push_back(append_new(tag_t::tr, {}));
        } else {
            while (open_.size() > row + 1) close_top();
        }
    }

    block_boundary();
    open_.push_back(append_new(tag, attributes));
}

void tree_builder::open(tag_t tag, attribute_span attributes) {
    if (tag == tag_t::tr || tag == tag_t::td || tag == tag_t::th) {
        open_in_table(tag, attributes);
        return;
    }

    // Anything else that starts where only rows and cells belong gets its
    // cell first -- a paragraph straight inside <tr>, a table in a table.
    foster_cell();

    if (tag == tag_t::br) {
        line_break();
        return;
    }
    if (tag == tag_t::img) {
        materialize_pending();
        flush_text();
        append_new(tag_t::img, attributes);
        has_content_ = true;
        return;
    }

    // A summary is the header of the details it opens in, and nothing
    // anywhere else: elsewhere the tag is dropped and its text stays.
    if (tag == tag_t::summary && open_.back()->tag() != tag_t::details) {
        record(error_t::misplaced_tag, canonical_name(tag));
        return;
    }

    if (is_block(tag)) {
        // A block tag inside inline markup closes the inlines by force.
        while (!is_block(open_.back()->tag()) && open_.back()->tag() != tag_t::root)
            close_top();

        // <li> closes the open item of the nearest list, if any.
        if (tag == tag_t::li) close_open_list_item();

        // Blocks that hold only inline content are closed by any block
        // start tag -- this is what makes </p> optional.
        for (;;) {
            const tag_t top = open_.back()->tag();
            if (top == tag_t::p || top == tag_t::h1 || top == tag_t::h2 ||
                top == tag_t::h3 || top == tag_t::pre || top == tag_t::summary) {
                close_top();
                continue;
            }
            break;
        }

        block_boundary();
        node* element = append_new(tag, attributes);
        // The rule is a block that holds nothing: it lands, it never opens.
        if (tag == tag_t::hr) return;
        open_.push_back(element);
        if (tag == tag_t::pre) {
            ++pre_depth_;
            pre_fresh_ = true;
            last_was_cr_ = false;
        }
        return;
    }

    // A link does not nest, in HTML or here: a new <a> closes the open
    // one first, the way browsers do.
    if (tag == tag_t::a) {
        for (std::size_t i = open_.size(); i-- > 1;) {
            if (open_[i]->tag() == tag_t::a) {
                close(tag_t::a);
                break;
            }
        }
    }

    // An ordinary inline: the pending space belongs before it.
    materialize_pending();
    flush_text();
    node* element = append_new(tag, attributes);
    open_.push_back(element);
}

void tree_builder::close_open_list_item() {
    for (std::size_t i = open_.size(); i-- > 1;) {
        const tag_t tag = open_[i]->tag();
        if (tag == tag_t::ul || tag == tag_t::ol) {
            for (std::size_t j = open_.size(); j-- > i + 1;) {
                if (open_[j]->tag() == tag_t::li) {
                    while (open_.size() > j) close_top();
                    return;
                }
            }
            return;
        }
    }
}

void tree_builder::close(tag_t tag) {
    // The matching open element, topmost first; a close with no open to
    // match is recorded and ignored.
    std::size_t found = open_.size();
    for (std::size_t i = open_.size(); i-- > 1;) {
        if (open_[i]->tag() == tag) {
            found = i;
            break;
        }
    }
    if (found == open_.size()) {
        record(error_t::unpaired_close, canonical_name(tag));
        return;
    }

    if (is_block(tag)) {
        while (open_.size() > found) close_top();
        return;
    }

    // A mismatched inline close, browser-fashion: everything above the
    // match is closed with it -- and the formatting elements among it
    // are reopened, so <b>first<i>second</b>third</i> leaves `third`
    // italic (design.md fixes exactly this example).
    xml::sta_vector<node*> reopen;
    for (std::size_t i = found + 1; i < open_.size(); ++i) {
        if (is_formatting(open_[i]->tag())) reopen.push_back(open_[i]);
    }
    if (!reopen.empty()) record(error_t::misnested_tags, canonical_name(tag));
    while (open_.size() > found) close_top();

    for (node* original : reopen) {
        // The attribute span is shared, not copied: both nodes live in
        // the same arena and neither ever writes it.
        node* fresh = append_new(original->tag(), original->attributes());
        open_.push_back(fresh);
    }
}

node* tree_builder::append_new(tag_t tag, attribute_span attributes) {
    node* element = arena_.create<node>(tag, attributes);
    open_.back()->append_child(*element);
    return element;
}

node& tree_builder::finish() {
    flush_text();
    return *root_;
}

}  // namespace wxl::html
