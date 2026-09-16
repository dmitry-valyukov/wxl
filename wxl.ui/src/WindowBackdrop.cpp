// The white behind a resized WinUI window, and who owns it.
//
// A WinUI3 top-level window is an ordinary redirected Win32 window -- no
// WS_EX_NOREDIRECTIONBITMAP -- whose class background brush is
// COLOR_WINDOW+1, the system white. The island lives in a child window
// (Microsoft.UI.Content.DesktopChildSiteBridge) that is resized in lockstep
// with the frame, but its composition runs a tick or two behind; in the gap
// the parent's redirection surface shows through, freshly erased with that
// white brush. Nothing composition-side can help: a compositor target on the
// HWND and the system-backdrop slot both sit *below* the redirection
// surface (both were tried, both lost -- the stripes stayed white).
//
// So the fix is the classic Win32 one: own the erase. The class brush is
// taken away, WM_ERASEBKGND is answered by painting the backdrop into the
// given DC, and WM_SIZE paints it synchronously inside the resize loop --
// before the framework's own handler runs, before DWM shows the new frame.
// The surface then never holds anything but the backdrop, and the gap
// continues the picture instead of flashing.
//
// GDI on purpose, which is why NOGDI is lifted for this one file: the
// redirection surface is a GDI surface, and painting it with anything else
// would mean another compositor -- the thing that just proved to be on the
// wrong side of it.
#undef NOGDI
#include <windows.h>

#include <commctrl.h>
#include <wincodec.h>
#include <winrt/base.h>

#include "WindowBackdrop.h"
#include "WindowHandle.h"

namespace wxl {
namespace {

/// What the window's surface is painted with: a solid color, or an image at
/// its natural size, stretched at paint time. `painted` and `fine` say what
/// the surface holds right now -- the size it was painted for and whether
/// with the slow filtered stretch -- so nothing paints twice for one size.
struct backdrop_state {
    HBITMAP image = nullptr;
    SIZE imageSize{};
    COLORREF color = 0;
    SIZE painted{};
    bool fine = false;
};

/// Leaked deliberately, entries, bitmaps and all: a backdrop lives exactly
/// as long as its window, and the process is on its way out anyway.
std::map<HWND, backdrop_state>& states() {
    static auto* const kept = new std::map<HWND, backdrop_state>;
    return *kept;
}

/// Cover, pinned to the top centre: proportions kept, the excess leaves
/// through the sides -- the same UniformToFill the XAML splash uses, so the
/// surface and the island agree about every pixel they both show.
///
/// Two speeds on purpose. HALFTONE filters per pixel on the CPU, and a
/// full-window pass of it on every WM_SIZE is exactly what makes a resize
/// drag feel heavy -- so the drag paints COLORONCOLOR, a plain scaled copy,
/// and the filtered pass runs once, when the size has settled.
void paint(HWND hwnd, HDC dc, backdrop_state& state, bool fine) {
    RECT client{};
    ::GetClientRect(hwnd, &client);
    if (client.right <= 0 || client.bottom <= 0) return;
    state.painted = {client.right, client.bottom};
    state.fine = fine;

    if (!state.image) {
        HBRUSH const brush = ::CreateSolidBrush(state.color);
        ::FillRect(dc, &client, brush);
        ::DeleteObject(brush);
        return;
    }

    double const scale = std::max(static_cast<double>(client.right) / state.imageSize.cx,
                                  static_cast<double>(client.bottom) / state.imageSize.cy);
    int const width = static_cast<int>(state.imageSize.cx * scale + 0.5);
    int const height = static_cast<int>(state.imageSize.cy * scale + 0.5);

    HDC const memory = ::CreateCompatibleDC(dc);
    HGDIOBJ const previous = ::SelectObject(memory, state.image);
    ::SetStretchBltMode(dc, fine ? HALFTONE : COLORONCOLOR);
    ::SetBrushOrgEx(dc, 0, 0, nullptr);
    ::StretchBlt(dc, (client.right - width) / 2, 0, width, height, memory, 0, 0,
                 state.imageSize.cx, state.imageSize.cy, SRCCOPY);
    ::SelectObject(memory, previous);
    ::DeleteDC(memory);
}

/// The whole point: the surface is repainted right here, synchronously
/// inside the resize loop, before the framework's own WM_SIZE work and
/// before DWM shows the frame at its new size -- the zero-latency path a
/// flicker-free Win32 application uses. Exactly once per size: the erase
/// that follows sees the surface already current and only says so, and the
/// filtered pass waits for WM_EXITSIZEMOVE.
LRESULT CALLBACK on_message(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR,
                            DWORD_PTR data) {
    auto* const state = reinterpret_cast<backdrop_state*>(data);
    switch (message) {
        case WM_ERASEBKGND: {
            RECT client{};
            ::GetClientRect(hwnd, &client);
            if (client.right != state->painted.cx || client.bottom != state->painted.cy) {
                paint(hwnd, reinterpret_cast<HDC>(wparam), *state, true);
            }
            return 1;
        }
        case WM_SIZE:
            if (wparam != SIZE_MINIMIZED) {
                HDC const dc = ::GetDC(hwnd);
                // Maximize is one event, not a drag: it can afford the
                // filtered pass, and no WM_EXITSIZEMOVE follows to run it.
                paint(hwnd, dc, *state, wparam == SIZE_MAXIMIZED);
                ::ReleaseDC(hwnd, dc);
            }
            break;
        case WM_EXITSIZEMOVE:
            if (!state->fine) {
                HDC const dc = ::GetDC(hwnd);
                paint(hwnd, dc, *state, true);
                ::ReleaseDC(hwnd, dc);
            }
            break;
    }
    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

backdrop_state& state_for(Window const& window) {
    HWND const hwnd = reinterpret_cast<HWND>(window_handle(window));
    std::map<HWND, backdrop_state>& all = states();
    if (auto const found = all.find(hwnd); found != all.end()) return found->second;

    // The class brush goes away so nothing fills the grown surface white on
    // the kernel's side of WM_SIZE. If a fill still races a DWM frame, it
    // now shows a moment of the previous paint -- the same picture, one
    // size stale -- instead of a white stripe.
    ::SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, 0);

    backdrop_state& state = all[hwnd];
    ::SetWindowSubclass(hwnd, on_message, 1, reinterpret_cast<DWORD_PTR>(&state));
    return state;
}

/// The image as a plain 32-bit DIB, decoded once at its natural size.
HBITMAP load_image(const wchar_t* path, SIZE& size) {
    winrt::com_ptr<IWICImagingFactory> wic;
    winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(wic.put())));

    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(wic->CreateDecoderFromFilename(path, nullptr, GENERIC_READ,
                                                        WICDecodeMetadataCacheOnLoad,
                                                        decoder.put()));
    winrt::com_ptr<IWICBitmapFrameDecode> frame;
    winrt::check_hresult(decoder->GetFrame(0, frame.put()));

    winrt::com_ptr<IWICFormatConverter> converter;
    winrt::check_hresult(wic->CreateFormatConverter(converter.put()));
    winrt::check_hresult(converter->Initialize(frame.get(), GUID_WICPixelFormat32bppBGR,
                                               WICBitmapDitherTypeNone, nullptr, 0.0,
                                               WICBitmapPaletteTypeMedianCut));

    UINT width = 0;
    UINT height = 0;
    winrt::check_hresult(converter->GetSize(&width, &height));

    // Top-down, so CopyPixels' first row is the bitmap's first row.
    BITMAPINFO info{};
    info.bmiHeader = {sizeof(BITMAPINFOHEADER), static_cast<LONG>(width),
                      -static_cast<LONG>(height), 1, 32, BI_RGB};
    void* bits = nullptr;
    HBITMAP const bitmap = ::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) winrt::throw_last_error();

    winrt::check_hresult(converter->CopyPixels(nullptr, width * 4, width * height * 4,
                                               static_cast<BYTE*>(bits)));
    size = {static_cast<LONG>(width), static_cast<LONG>(height)};
    return bitmap;
}

/// Paints once right away: the surface must be right from the first frame,
/// not from the first resize.
void repaint_now(Window const& window, backdrop_state& state) {
    HWND const hwnd = reinterpret_cast<HWND>(window_handle(window));
    HDC const dc = ::GetDC(hwnd);
    paint(hwnd, dc, state, true);
    ::ReleaseDC(hwnd, dc);
}

}  // namespace

void window_backdrop_color(Window const& window, Color color) {
    backdrop_state& state = state_for(window);
    if (state.image) ::DeleteObject(state.image);
    state.image = nullptr;
    state.color = RGB(color.R, color.G, color.B);
    repaint_now(window, state);
}

void window_backdrop_image(Window const& window, const wchar_t* imagePath) {
    SIZE size{};
    HBITMAP const image = load_image(imagePath, size);

    backdrop_state& state = state_for(window);
    if (state.image) ::DeleteObject(state.image);
    state.image = image;
    state.imageSize = size;
    repaint_now(window, state);
}

}  // namespace wxl
