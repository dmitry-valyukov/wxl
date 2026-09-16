#pragma once

// wxl::FormattedBlock -- a RichTextBlock with a procedural filling
// vocabulary: the common base every markup control (wxl.html's HtmlBlock,
// future BB/RSDN blocks) builds its content through, so the parsers differ
// and the way runs, links and images become XAML text elements is written
// once. The spec that shaped it is wxl.html/design.md.
//
// The primitives are deliberately flat: no open-element state survives a
// call. A parser walks its own tree and says "this text, styled so",
// "a link here", "new paragraph" -- and each call is complete in itself,
// which is what lets content arrive in independent chunks (a chat feed
// appends; nothing is reparsed).
//
// Every text parameter is a view with a zero right after it: the view goes
// into WinRT as a string reference (winrt::param::hstring's contract, and
// a deliberate abort() when broken -- test/param_hstring_test.cpp), and no
// copy is bought here to insure against the caller. Literals, any
// basic_string and wxl.html's arena views satisfy the contract for free.

#include <cstdint>
#include <optional>
#include <string>
#include <string_view>

#include "Thickness.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "geometry.h"

namespace wxl {

// std::wstring with the STA pool's allocator -- the wchar_t sibling of the
// generated wxl::wstring (which is char16_t, HSTRING's unit). Styles are
// created per parsed fragment, so their string lives on the pool like every
// other repeating allocation on the one STA thread.
using sta_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>,
                                      core::sta_allocator<wchar_t>>;

// One level of inline styling, worn over whatever is outside it: an empty
// field inherits, a filled one overrides. pushStyle() puts a level on,
// popStyle() takes the innermost off -- the stack is what nested <b><i>
// or nested quote colors flatten into.
struct TextStyle {
    std::optional<Color> color;
    std::optional<double> fontSize;
    std::optional<sta_wstring> fontFamily;
    std::optional<bool> bold;
    std::optional<bool> italic;
    std::optional<bool> underline;
    std::optional<bool> strikethrough;
};

// What a paragraph itself can wear: the block-level facets, worn once at
// appendParagraph() rather than stacked -- a paragraph has one margin and
// one alignment, there is nothing to merge.
struct BlockStyle {
    std::optional<Thickness> margin;
    std::optional<TextAlignment> alignment;
};

// What the diagnostic sink can be told about. Every one of these is a
// recovery the stack performs *by design* -- content is foreign, and the
// text always survives -- so none of them fails anything; the record is
// for the developer wondering why the output is not what the markup said.
// Named for its first and main source; a future BB/RSDN parser pours the
// same kinds into the same sink.
enum class HtmlErrorKind : std::uint8_t {
    UnknownTag,     // tag outside the subset dropped, its content kept
    UnpairedClose,  // close tag with nothing open to match, ignored
    MisnestedTags,  // close matched below the top; recovered browser-fashion
    BadEntity,      // &name;-shaped body that failed to decode, left literal
    MisplacedTag,   // a row or cell with no table around it, a summary outside details: dropped
    UnknownStyle,   // style="..." name not in the registry (real CSS lands here)
    BadColor,       // color attribute that is neither #hex nor a known name
    BadFontSize,    // <font size> outside the 1..7 / +n/-n grammar
    BadDimension,   // width/height that is not a number
    MissingImage,   // <img> without a usable src
    RemoteImage,    // non-local image source dropped by policy
    ImageFailed,    // the image existed but did not load (fires asynchronously)
    BlockedLink,    // link click dropped by the default policy (non-web scheme)
};

// One diagnostic record. The detail names the offender -- tag name, style
// name, attribute value, image source -- and, like an event-args view, is
// valid only inside the handler call: it points into storage the caller
// releases right after.
struct HtmlError {
    HtmlErrorKind kind;
    std::wstring_view detail;
};

// How a wide element (appendWideElement) takes the width of the text area:
// always, or only while the block wraps text.
enum class WideWidth : std::uint8_t {
    Always,
    WhileWrapping,
};

class FormattedBlock : public RichTextBlock {
    using base_t = RichTextBlock;

public:
    class Impl;

    FormattedBlock();

    template <typename... Setters>
        requires impl::setter_pack<FormattedBlock, Setters...>
    explicit FormattedBlock(Setters&&... setters) : FormattedBlock() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    // Everything appended goes to the *last* paragraph; the appenders make
    // one when there is none yet, so bare text works without ceremony.

    // Removes all content and forgets the style stack.
    void clear() const;

    // Opens a new paragraph; the appends that follow land in it.
    void appendParagraph() const;

    // The same, dressed: margin and alignment are the paragraph's own.
    void appendParagraph(BlockStyle const& style) const;

    // Text in the current effective style (the merge of the pushed levels).
    void appendText(std::wstring_view text) const;

    void appendLineBreak() const;

    // A link. Clicks go to the onLink handler; without one, an http/https
    // target opens in the default browser and every other scheme is
    // silently dropped -- the target came with foreign content.
    void appendLink(std::wstring_view text, std::wstring_view target) const;

    // The same link as a scope, for content richer than one flat run:
    // between pushLink() and popLink(), appendText goes inside the link and
    // wears the style stack as usual. What cannot go inside is anything
    // carried by an InlineUIContainer (images, sub/sup) or a line break --
    // a Hyperlink admits only runs and plain spans -- so the link *splits*
    // around it: the element sits between two halves that share the target,
    // and the text keeps its order. A second pushLink closes the first:
    // links do not nest, in HTML or here.
    void pushLink(std::wstring_view target) const;
    void popLink() const;

    // Text in its own small block riding the baseline: the sub/sup trick
    // the spec settled on, since OpenType variants cover almost no letters
    // and a Run cannot shift vertically. `scale` is the size against the
    // current effective one; `drop` shifts down in fractions of that size
    // (negative lifts).
    void appendScript(std::wstring_view text, double scale, double drop) const;

    // An image from a local file or resource URI. The size is required by
    // design (wxl.html/design.md): the bitmap loads asynchronously, and a
    // line that grows after the fact is a jump the reader sees. toolTip
    // doubles as the automation name, same as the toolTip tag everywhere.
    void appendImage(std::wstring_view source, Size size,
                     std::wstring_view toolTip = {}) const;

    // Any element inline with the text -- the door <sub>/<sup> tricks
    // come through.
    void appendElement(UIElement const& element) const;

    // An element on a line of its own, as wide as the text area: a rule, a
    // table, the header of a folded region. The block keeps it that wide
    // through every resize -- the width a browser gives a block box. With
    // WhileWrapping the element goes back to its natural width under
    // TextWrapping::NoWrap, like everything else on an unwrapped line; a
    // rule has no natural width and asks for Always.
    void appendWideElement(FrameworkElement const& element,
                           WideWidth width = WideWidth::WhileWrapping) const;

    // A folded region. The paragraphs appended between beginFold() and
    // endFold() leave the block at endFold() and come back -- in place,
    // right after the paragraph the toggle sits in -- whenever the toggle
    // expands; collapsing takes them out again. They stay this block's own
    // paragraphs, nothing is nested, so selection and search run through
    // them while they are shown. The caller appends the toggle itself, as a
    // wide element, before beginFold().
    void beginFold(Expander const& toggle) const;
    void endFold() const;

    // Wrapping is the RichTextBlock's own property, taken over so that a
    // change refits the wide elements at once rather than at the next
    // resize.
    using base_t::textWrapping;
    void textWrapping(TextWrapping value) const;

    void pushStyle(TextStyle const& style) const;
    void popStyle() const;

    // One handler for every link in the block, current and future. Stored
    // as wxl's own core::function -- one allocation with the capture laid
    // out inside it, unlike std::function's separate capture storage. Any
    // callable of the right shape converts to one, so that is what a caller
    // writes; a callable of the wrong shape is refused at the call, by the
    // constraint on that conversion.
    using LinkHandler = core::function<void(std::wstring_view)>;

    void onLink(LinkHandler handler) const;

    // The diagnostic sink: every recovery of every parser filling this
    // block, and the block's own late failures (an image that would not
    // load, a link click the default policy dropped). Called synchronously
    // during the fill for build-time kinds, and from the UI thread later
    // for the asynchronous ones. Without a handler, everything stays
    // silent -- exactly as before.
    using ErrorHandler = core::function<void(HtmlError const&)>;

    void onError(ErrorHandler handler) const;

protected:
    explicit FormattedBlock(Impl* impl) noexcept;

    friend class Object::Impl;
};

}  // namespace wxl
