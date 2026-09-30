#pragma once

#include "hstring_param.h"

// wxl::Geometry -- a shape, written the way XAML writes it: the path
// mini-language, "F1 M 20,20 L 24,10 L 24,24 L 5,24", or the short forms of
// it the framework's own converter knows.
//
// A hand-written type rather than a wrapper over Microsoft.UI.Xaml.Media
// .Geometry: the runtime type is an abstract base whose concrete children
// (PathGeometry, with figures and segments) are built by the framework's own
// converter from exactly this text, and a declarative UI only ever names one.
// wxl keeps the text and has the converter build the real geometry on the way
// into the property, which keeps `data = u"F1 M 20,20 L 24,10"` to those
// characters.
//
// The text is an hstring, a reference to a string of its own: one made from
// text hands the same reference on -- a count, not a copy. A geometry that
// came back from the framework has no text to give (the runtime object is a
// tree of figures, not the string it was made from), and reads as empty.

namespace wxl {

class Geometry {
public:
    Geometry() = default;

    /// A path already owned: the reference is taken over, nothing is copied.
    Geometry(hstring data) noexcept : data_(std::move(data)) {}

    /// A path that is only borrowed: kept as one -- a count when it was an
    /// hstring, a copy of the text when it lay under a fast-pass header.
    Geometry(const hstring_param& data) : data_(data) {}

    /// Not explicit, on purpose: a string literal is a shape wherever a
    /// property asks for one. A template, not one more overload, because
    /// through `Geometry(hstring_param)` a literal would take two
    /// user-defined conversions, which an implicit sequence never has.
    template <typename Text>
        requires(!std::same_as<Text, hstring> && !std::same_as<Text, hstring_param> &&
                 std::convertible_to<const Text&, hstring_param>)
    Geometry(const Text& data) : data_(hstring_param(data)) {}

    const hstring& data() const noexcept { return data_; }

    bool empty() const noexcept { return data_.empty(); }

private:
    hstring data_;
};

}  // namespace wxl
