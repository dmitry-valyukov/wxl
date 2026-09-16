#pragma once

// Reading a window's place back, which the declarative side cannot do.
//
// `Window { placement = text }` puts a window where the text says; this is
// the other direction, and it is a free function rather than a property
// because a property tag is a way of *setting* something -- there is no
// braced form of asking.
//
// The text is opaque and meant to stay that way: an application writes it to
// its settings and hands it back next time unread. What it looks like, and
// what happens when a remembered monitor is gone, is in
// impl/window_placement.h.

#include "core.h"
#include "generated/collections.h"

namespace wxl {

class Window;

/// Where the window is now, in the form `placement` takes back.
///
/// Worth reading before going full-screen rather than after: it reports the
/// rectangle the window returns to, and a window told to cover the display
/// has been told nothing about where that is.
wstring window_placement(Window const& window);

}  // namespace wxl
