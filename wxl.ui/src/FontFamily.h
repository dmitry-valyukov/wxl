#pragma once

#include "hstring_param.h"

// wxl::FontFamily -- a font is named, and naming it is all a declarative UI
// ever does with one.
//
// A hand-written type rather than a wrapper over Microsoft.UI.Xaml.Media
// .FontFamily: the runtime type is a COM object built from that name and
// offering nothing else worth reaching, so wxl keeps the text and builds the
// real one on the way into the property. That is what makes
// `fontFamily = u"Cascadia Mono"` exactly those characters.
//
// The name is what XAML writes too, including its packaged form for a font
// shipped with the application -- "ms-appx:///Assets/Fonts/Name.ttf#Family".
//
// The name is an hstring, a reference to a string of its own: a family read
// off a control shares the HSTRING the control holds, and one made from a
// family hands the same reference on.

namespace wxl {

class FontFamily {
public:
    FontFamily() = default;

    /// A name already owned: the reference is taken over, nothing is copied.
    FontFamily(hstring name) noexcept : name_(std::move(name)) {}

    /// A name that is only borrowed: kept as one -- a count when it was an
    /// hstring, a copy of the text when it lay under a fast-pass header.
    FontFamily(const hstring_param& name) : name_(name) {}

    /// Not explicit, on purpose: the whole point is that a string literal is
    /// a font wherever a property asks for one. A template, not one more
    /// overload, because through `FontFamily(hstring_param)` a literal or a
    /// string would take two user-defined conversions, which an implicit
    /// sequence never has.
    template <typename Text>
        requires(!std::same_as<Text, hstring> && !std::same_as<Text, hstring_param> &&
                 std::convertible_to<const Text&, hstring_param>)
    FontFamily(const Text& name) : name_(hstring_param(name)) {}

    const hstring& name() const noexcept { return name_; }

    bool empty() const noexcept { return name_.empty(); }

private:
    hstring name_;
};

}  // namespace wxl
