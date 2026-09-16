#pragma once

#include "core.h"

// wxl::accept_file_drops -- files dragged onto a window, as a list of paths.
//
// XAML has drag and drop of its own, and it is not this: it speaks
// DataPackageView and hands out storage items through an asynchronous WinRT
// call, which is a whole apparatus for what is, here, one question -- which
// files did they drop? The shell answers that question directly, and has since
// before OLE existed: a window marked with DragAcceptFiles is sent
// WM_DROPFILES with the paths already in it. So this is DragAcceptFiles plus a
// window subclass to catch the message, and nothing else.
//
// One handler per window; registering again replaces it. The registration
// lasts as long as the window does.
//
// The paths arrive as wide strings because that is what the file system
// speaks; whether any of them is a file this application wants is the
// handler's business, and refusing them all is a perfectly good answer.

#include "generated/collections.h"

namespace wxl {

class Window;

/// Calls `handler` with the dropped paths. Returns false if the window has no
/// handle yet, or if the subclass could not be installed.
bool accept_file_drops(Window const& window,
                       std::function<void(std::vector<std::wstring> const&)> handler);

}  // namespace wxl
