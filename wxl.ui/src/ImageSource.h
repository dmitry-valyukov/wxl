#pragma once

#include <concepts>

#include "Object.h"
#include "Uri.h"

// wxl::ImageSource -- where an image comes from, which in a declarative UI
// is a path and nothing else.
//
// Projected onto Microsoft.UI.Xaml.Media.ImageSource rather than wrapped
// (see wxl.gen/gen/projection.cpp): the runtime type is an abstract base
// whose only construction is a concrete subclass, so a generated wrapper
// for it could express nothing at all. This holds the text and builds the
// real BitmapImage on the way into the property, which keeps
// `source = u"Assets/logo.png"` to exactly those characters.
//
// A path without a scheme is the application's own file, resolved next to
// the executable -- the folder a build puts its assets in, packaged or not.
//
// The other thing an image source can be is a bitmap the application made
// itself (a BitmapImage given a stream): then it is that very object and
// nothing is loaded.

namespace wxl {

class ImageSource {
public:
    ImageSource() = default;

    /// Not explicit, on purpose: a string literal is an image source
    /// wherever a property asks for one. Anything that is a Uri is one; any
    /// string is a Uri on its own (see Uri.h), and this is a template for the
    /// same reason that one is -- a literal would otherwise take two
    /// user-defined conversions.
    ImageSource(Uri source) noexcept : source_(std::move(source)) {}

    template <typename Text>
        requires(!std::same_as<Text, Uri> && std::convertible_to<const Text&, Uri>)
    ImageSource(const Text& text) : source_(text) {}

    /// A bitmap the application made: handed to the property as it is.
    template <typename Bitmap>
        requires std::derived_from<Bitmap, Object>
    ImageSource(const Bitmap& bitmap) : bitmap_(static_cast<const Object&>(bitmap)) {}

    const Uri& source() const noexcept { return source_; }

    /// The bitmap, when this source is one and not a path.
    const Object* bitmap() const noexcept { return bitmap_ ? &*bitmap_ : nullptr; }

    bool empty() const noexcept { return source_.empty() && !bitmap_; }

private:
    Uri source_;
    core::nullable<Object> bitmap_;
};

}  // namespace wxl
