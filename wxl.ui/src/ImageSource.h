#pragma once

#include "Uri.h"

// wxl::ImageSource -- where an image comes from, which in a declarative UI
// is a path and nothing else.
//
// Projected onto Microsoft.UI.Xaml.Media.ImageSource rather than wrapped
// (see wxl.gen/gen/projection.cpp): the runtime type is an abstract base
// whose only construction is a concrete subclass, so a generated wrapper
// for it could express nothing at all. This holds the text and builds the
// real BitmapImage on the way into the property, which keeps
// `source = L"Assets/logo.png"` to exactly those characters.
//
// A path without a scheme is the application's own file, resolved next to
// the executable -- the folder a build puts its assets in, packaged or not.

namespace wxl {

class ImageSource {
public:
    ImageSource() = default;

    // Not explicit, on purpose: a string literal is an image source
    // wherever a property asks for one. The pointer overload is what makes
    // the literal itself work -- through the view alone it would take two
    // user-defined conversions, which an implicit sequence never has.
    ImageSource(Uri source) noexcept : source_(std::move(source)) {}
    ImageSource(std::wstring_view text) noexcept : source_(text) {}
    ImageSource(wchar_t const* text) noexcept : source_(text) {}
    ImageSource(std::u16string_view text) noexcept : source_(text) {}
    ImageSource(char16_t const* text) noexcept : source_(text) {}

    Uri const& source() const noexcept { return source_; }

    bool empty() const noexcept { return source_.empty(); }

private:
    Uri source_;
};

}  // namespace wxl
