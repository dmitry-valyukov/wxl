#pragma once

// wxl::HtmlBlock -- rich text from the HTML subset: forum posts, chat
// messages, help pages. The parser is the module wxl.html (its contract --
// tolerant to any input, self-contained chunks -- is wxl.html/design.md);
// this control is the join: markup in, the family builder out. The look
// (theme), the image base directory and the onError sink come from
// MarkupBlock, shared with the sibling markups BbBlock and RsdnBlock.
//
// Content arrives two ways, both after construction:
//   - html(markup) replaces everything;
//   - append(markup) adds to the end -- a chat feed appends, nothing is
//     reparsed. Every insertion closes all its blocks: an unclosed <b>
//     never leaks into the next chunk, because neither the source nor any
//     parser state survives a call.
//
// The style attribute carries the *name* of a registered style, never CSS:
// <div style="quote"> finds what registerStyle(L"quote", ...) put in, and
// an unknown name -- real CSS from foreign markup included -- is ignored
// with an onError record, per the spec's "the text is sacred" rule.

#include <initializer_list>
#include <string_view>
#include <utility>

#include "MarkupBlock.h"

namespace wxl {

class HtmlBlock : public MarkupBlock {
    using base_t = MarkupBlock;

public:
    class Impl;

    HtmlBlock();

    template <typename... Setters>
        requires impl::setter_pack<HtmlBlock, Setters...>
    explicit HtmlBlock(Setters&&... setters) : HtmlBlock() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    // Replaces the whole content with the parsed markup.
    //
    // Parsed against the font size the block has at the time: <sub>, <sup>
    // and the headings are fractions of it, fixed as the runs are built. So
    // the size is set before the markup, not after -- in the declarative
    // form, where setters are applied left to right, the markup goes last.
    void html(std::wstring_view markup) const;

    // Parses the markup and adds it after what is already there: bare
    // inline content continues the last paragraph, block tags open new
    // ones. The insertion is complete in itself -- see above.
    void append(std::wstring_view markup) const;

    // The named styles style="..." resolves against. Registering a name
    // again replaces it; already-built content keeps the look it was built
    // with -- the registry is read at parse time, not live. One name per
    // attribute, by decision -- foreign markup's real CSS in style="..."
    // simply never finds a match.
    void registerStyle(std::wstring_view name, HtmlStyle const& style) const;
    void registerStyles(
        std::initializer_list<std::pair<std::wstring_view, HtmlStyle>> styles) const;

    // A bare string in the declarative form is the markup:
    // HtmlBlock{ L"Привет, <b>мир</b>!" }.
    using base_t::setPositional;
    void setPositional(std::wstring_view markup) const { html(markup); }

protected:
    explicit HtmlBlock(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
