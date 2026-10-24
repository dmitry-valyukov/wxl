#pragma once

// wxl::WindowNative -- what Win32 still owns of a WinUI window, as .NET's
// WinRT.Interop.WindowNative names it.
//
// WinUI3 is a layer over Win32 and never claims otherwise: a file dialog, a task-bar call, a
// shell interface all want a window handle, there is no WinUI way to give them one, and several
// things a desktop window plainly has -- an owner, files dropped on it, the colour behind its
// island, the rectangle it returns to -- WinUI3 has no word for. So the handle is handed out, and
// those things are done here the way Win32 does them, through the handle. What WinUI itself can
// do for a window stays out (sizeToContent, a tag of Window).
//
// Static members alone: what wxl keeps of a window here it keeps by handle, in one store, and
// one subclass procedure answers the window's messages for all of it -- the backdrop paints, the
// dropped files arrive, and the window going takes its entry with it. TrayIcon keeps a window of
// its own and stays apart.
//
// The handle is spelled as the pointer HWND actually is, so a caller that has included
// windows.h passes the result straight into Win32 with no cast, and one that has not still
// compiles -- an incomplete type is enough to name a pointer to it. No Windows header here.

#include "Color.h"
#include "core.h"
#include "hstring_param.h"

struct HWND__;

namespace wxl {

class Window;

namespace impl {

struct window_placement_setter;

}  // namespace impl

class WindowNative {
public:
    WindowNative() = delete;

    /// The window's handle, or nullptr before the window is created. For what an application
    /// needs of Win32 and wxl has no opinion about.
    static HWND__* handle(Window const& window);

    /// Where the window is now, in the form `Window { placement = text }` takes back. Read here
    /// rather than through a property: a property tag is a way of *setting* something, and there
    /// is no braced form of asking. The text is opaque and meant to stay that way: an
    /// application writes it to its settings and hands it back next time unread (the form is in
    /// impl/window_helpers.h).
    ///
    /// Worth reading before going full-screen rather than after: it reports the rectangle the
    /// window returns to, and a window told to cover the display has been told nothing about
    /// where that is.
    static hstring placement(Window const& window);

    /// Paints what shows where the XAML island has not drawn yet with one solid color. The
    /// island repaints on its own schedule: WM_SIZE starts a layout pass, and the frame at the new
    /// size reaches its compositor a tick or two after DWM has already moved the window's edge.
    /// In that gap the window's own redirection surface shows through, erased with the class
    /// brush WinUI registers as system white -- the white stripes a fast drag of the corner
    /// leaves. The class brush is removed, and the backdrop is painted into the surface with GDI,
    /// synchronously in WM_SIZE and in WM_ERASEBKGND. No compositor on purpose: a composition
    /// layer sits below the redirection surface and can never cover its white.
    static void setBackdropColor(Window const& window, Color color);

    /// The same with an image: decoded once at its natural size, stretched at paint time as
    /// UniformToFill pinned to the top centre -- the way the splash screen's own XAML Image is
    /// placed, so the two agree about every pixel they both show. A terminated path, because that
    /// is what WIC's file decoder takes.
    static void setBackdropImage(Window const& window, const wchar_t* imagePath);

    /// Calls `handler` with the paths of the files dragged onto the window. Not XAML's drag and
    /// drop, which speaks DataPackageView and storage items through an asynchronous call: the
    /// shell answers "which files did they drop?" directly, and a window marked with
    /// DragAcceptFiles is sent WM_DROPFILES with the paths already in it. One handler per window;
    /// registering again replaces it, and it lasts as long as the window does. False if the
    /// window has no handle yet, or the subclass could not be installed.
    static bool acceptFileDrops(Window const& window,
                                std::function<void(std::vector<std::wstring> const&)> handler);

    /// Makes `dialog` modal to `owner` -- the owner is disabled until the dialog closes -- and
    /// keeps it off the taskbar and out of Alt+Tab, with minimize and maximize turned off: WinUI3
    /// gives a Window no modal mode and no owner. The owner is set the Win32 way, and WinUI's
    /// OverlappedPresenter.IsModal disables it. Both windows must already be created; call it
    /// before activating `dialog`.
    static void makeModal(Window const& dialog, Window const& owner);

private:
    friend struct impl::window_placement_setter;

    /// What wxl keeps of one window -- the rectangle it returns to, its backdrop, the handler of
    /// its dropped files -- and the one subclass procedure that answers its messages. Both speak
    /// Win32, so both are defined in WindowNative.cpp. Only what keeps something of its own or
    /// catches a message makes an entry; reading never does.
    struct native_state;
    struct subclass;

    /// The window's entry in the store, made on first ask together with the subclass that takes
    /// it away when the window goes; null for no handle, or a subclass that could not be put on.
    static native_state* state_of(HWND__* hwnd);
};

}  // namespace wxl
