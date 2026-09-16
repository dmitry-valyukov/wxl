#pragma once

#include "core.h"

// wxl::window_handle -- the HWND behind a window.
//
// WinUI3 is a layer over Win32 and never claims otherwise: a file dialog, a
// task-bar call, a shell interface all want a window handle, and there is no
// WinUI way to give them one. So the handle is handed out rather than hidden,
// and what it is for stays the caller's business.
//
// The return type is spelled as the pointer HWND actually is, so a caller
// that has included windows.h passes the result straight into Win32 with no
// cast, and one that has not still compiles -- an incomplete type is enough
// to name a pointer to it.
//
// Everything wxl itself needs from Win32 it does inside its own translation
// units (window placement is the standing example, WindowPlacement.h). This
// is for what an application needs and wxl has no opinion about.

struct HWND__;

namespace wxl {

class Window;

/// The window's handle, or nullptr before the window is created.
HWND__* window_handle(Window const& window);

}  // namespace wxl
