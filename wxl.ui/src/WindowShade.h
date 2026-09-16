#pragma once

#include "core.h"
#include "geometry.h"

// wxl::WindowShade -- a window that flies to a point and back, on the
// compositor.
//
// Minimizing to the notification area, and the like: the window's own pixels
// shrink and fade toward a point on the screen -- a tray icon, say -- and the
// window is hidden there; expanding reverses it. The motion is a composition
// visual on the DWM thread, not frames the application draws, so it stays
// smooth while the thread is busy, the same way a page turn does.
//
// It animates a *snapshot* of the window, taken the moment it starts to
// collapse, on a transparent click-through overlay of its own; the real
// window is hidden for the flight and shown again at the end. So the window
// need not be ours to animate -- a handle is enough -- and nothing it hosts
// (a console, here) has to cooperate.
//
// One shade per window, kept for as long as the window is managed this way:
// the overlay and the compositor behind it are made once and reused. Where no
// compositor is available the calls still hide and show the window, only
// without the animation -- so a caller never has to check.

// Declared, not included: what is kept is a window handle, and a caller that
// only wants to minimize a window should not have to parse a Windows header.
struct HWND__;

namespace wxl {

class WindowShade {
public:
    /// Binds to the window whose pixels will fly. The overlay and compositor
    /// are built here, once.
    explicit WindowShade(HWND__* window);
    ~WindowShade();

    WindowShade(WindowShade&&) noexcept;
    WindowShade& operator=(WindowShade&&) noexcept;
    WindowShade(WindowShade const&) = delete;
    WindowShade& operator=(WindowShade const&) = delete;

    /// Hides the window and flies its snapshot down to `screenPoint` (screen
    /// pixels), shrinking and fading. `whenDone` runs once the flight is over;
    /// it may be empty.
    void collapseTo(Point screenPoint, std::function<void()> whenDone = {}) const;

    /// Flies the snapshot up from `screenPoint` back to the window's place and
    /// shows the window. `whenDone` runs once it is back and shown -- where a
    /// caller brings it to the front; it may be empty.
    void expandFrom(Point screenPoint, std::function<void()> whenDone = {}) const;

private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};

}  // namespace wxl
