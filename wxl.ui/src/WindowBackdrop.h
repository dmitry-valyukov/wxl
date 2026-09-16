#pragma once

#include "Color.h"

// wxl::window_backdrop_* -- what shows where the XAML island has not drawn
// yet.
//
// The island repaints on its own schedule: WM_SIZE starts a layout pass, and
// the frame at the new size reaches its compositor a tick or two after DWM
// has already moved the window's edge. In that gap the top-level window's
// own redirection surface shows through -- and Win32 erases it with the
// window class's background brush, which WinUI registers as system white.
// Hence the white stripes a fast drag of the corner leaves, in a light
// theme and a dark one alike.
//
// These calls take that surface over: the class brush is removed, and the
// backdrop -- a color or an image -- is painted into the surface with GDI,
// synchronously in WM_SIZE and in WM_ERASEBKGND, through a window subclass.
// No compositor is involved on purpose: a composition layer sits below the
// redirection surface and can never cover its white (both the
// system-backdrop slot and a compositor target on the HWND were tried and
// lost). The island draws over this as it always did; the backdrop shows
// only where the island has not.

namespace wxl {

class Window;

/// Paints the window's backdrop with one solid color.
void window_backdrop_color(Window const& window, Color color);

/// Paints the window's backdrop with an image: decoded once at its natural
/// size, stretched at paint time as UniformToFill pinned to the top centre
/// -- the way the splash screen's own XAML Image is placed, so the two
/// agree about every pixel they both show. A terminated path, because that
/// is what WIC's file decoder takes.
void window_backdrop_image(Window const& window, const wchar_t* imagePath);

}  // namespace wxl
