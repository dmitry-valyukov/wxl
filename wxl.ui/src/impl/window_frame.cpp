#include "window_frame.h"

// dwmapi.h includes uxtheme.h, which names LOGFONTW and RGBQUAD: GDI types,
// which NOGDI would otherwise keep out of windows.h.
#undef NOGDI
#include <windows.h>

#include <dwmapi.h>

namespace wxl::impl {

void frame_dark(HWND__* hwnd, bool on) noexcept {
    BOOL const dark = on ? TRUE : FALSE;
    ::DwmSetWindowAttribute(hwnd, DWMWA_USE_IMMERSIVE_DARK_MODE, &dark, sizeof(dark));
}

void frame_caption(HWND__* hwnd, uint32_t face, uint32_t ink) noexcept {
    COLORREF const caption = face;
    COLORREF const text = ink;
    ::DwmSetWindowAttribute(hwnd, DWMWA_CAPTION_COLOR, &caption, sizeof(caption));
    ::DwmSetWindowAttribute(hwnd, DWMWA_TEXT_COLOR, &text, sizeof(text));
}

}  // namespace wxl::impl
