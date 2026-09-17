#pragma once

// The DWM side of a window's frame: the dark frame and the caption colours.
//
// A file of its own, and not a pair of lines in CompositionWindow.cpp, because
// dwmapi.h drags uxtheme.h in, and that one wants the GDI types NOGDI keeps
// out of the rest of the library. NOGDI is lifted here alone, the way
// WindowBackdrop.cpp lifts it for the redirection surface, and the window's
// own translation unit stays without GDI.
//
// Declared with a forward HWND and plain integers, so the window can call it
// without dwmapi.h of its own: a COLORREF is an unsigned 0x00BBGGRR.

#include <cstdint>

struct HWND__;

namespace wxl::impl {

/// DWMWA_USE_IMMERSIVE_DARK_MODE: the frame in the dark theme, or back in the
/// light one.
void frame_dark(HWND__* hwnd, bool on) noexcept;

/// DWMWA_CAPTION_COLOR and DWMWA_TEXT_COLOR, as COLORREFs. Windows 11; where
/// DWM does not know them the call has no effect and the frame stays the
/// system's, which is the only sensible outcome of the refusal.
void frame_caption(HWND__* hwnd, uint32_t face, uint32_t ink) noexcept;

}  // namespace wxl::impl
