#pragma once

// wxl::Uri -- what a declarative UI ever does with a URI, which is name one.
//
// A hand-written type rather than a wrapper over Windows.Foundation.Uri: the
// runtime type is a full COM object with a parser behind it, and a property
// that takes one is written in the DSL as a string literal and nothing else.
// Holding the text and building the real Uri on the way into the property
// keeps `NavigateUri = u"https://..."` to exactly those characters.
//
// The text is an hstring, a reference to a string of its own: a Uri read off a
// control shares the HSTRING the control holds, and one made from a Uri hands
// the same reference on -- a count, not a copy.

#include "hstring_param.h"

namespace wxl {

class Uri {
public:
    Uri() = default;

    /// A string already owned: the reference is taken over, nothing is copied.
    Uri(hstring text) noexcept : text_(std::move(text)) {}

    /// A string that is only borrowed: kept as one -- a count when it was an
    /// hstring, a copy of the text when it lay under a fast-pass header.
    Uri(const hstring_param& text) : text_(text) {}

    /// Not explicit, on purpose: the whole point is that a string literal is
    /// a URI wherever a property asks for one. What counts as a string is
    /// hstring_param's business and nobody else's here. It is a template, not
    /// one more overload, because through `Uri(hstring_param)` a literal would
    /// take two user-defined conversions (literal to hstring_param, then to
    /// Uri), which an implicit sequence never has; deduced, the literal
    /// arrives as itself and hstring_param is the one conversion inside.
    template <typename Text>
        requires(!std::same_as<Text, hstring> && !std::same_as<Text, hstring_param> &&
                 std::convertible_to<const Text&, hstring_param>)
    Uri(const Text& text) : text_(hstring_param(text)) {}

    const hstring& text() const noexcept { return text_; }

    bool empty() const noexcept { return text_.empty(); }

private:
    hstring text_;
};

}  // namespace wxl
