#pragma once

// Which sides of a window follow its content: the `sizeToContent` tag of Window, WPF's
// Window.SizeToContent with WPF's values. WinUI opens a desktop window at a large default and offers
// no size-to-content of its own -- a Flyout sizes to its content, a Window does not. A header of its
// own, because the DSL tag names the type in Members.h, which a class that includes Members.h could
// not provide.

#include <cstdint>

namespace wxl {

enum class SizeToContent : int32_t {
    Manual,
    Width,
    Height,
    WidthAndHeight,
};

}  // namespace wxl
