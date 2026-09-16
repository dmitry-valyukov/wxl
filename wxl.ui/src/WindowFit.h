#pragma once

// wxl::fitToContent -- a window sized to what it holds.
//
// WinUI opens a desktop window at a large default and offers no size-to-content
// of its own -- a Flyout sizes to its content, a Window does not. This is that
// for a window: once the content has been laid out, the window is resized to it.
// It is the manual form of what a Flyout does automatically, and the reason a
// dialog built as a second window still fits its contents like a popup would.
//
// One caveat the caller carries: a vertical stack of stretch controls (a form of
// text fields) has no width of its own -- the fields collapse to their minimum --
// so give the content a width before calling this, and only the height is left to
// the content. The window is grown by a fixed allowance over the content for the
// title bar and border, which WinUI does not hand out a measurement of.

#include "core.h"

namespace wxl {

class Window;

/// Resizes `window` to its content once the content is laid out (on the content's
/// Loaded). Wires a one-time handler and returns at once; the resize happens when
/// the window is activated and its content loads.
void fitToContent(Window const& window);

}  // namespace wxl
