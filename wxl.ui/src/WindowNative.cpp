// What Win32 still owns of a WinUI window: its handle, the place it returns to, the surface behind
// its island, the files dropped on it and its owner.
//
// The backdrop. A WinUI3 top-level window is an ordinary redirected Win32 window -- no
// WS_EX_NOREDIRECTIONBITMAP -- whose class background brush is COLOR_WINDOW+1, the system white.
// The island lives in a child window (Microsoft.UI.Content.DesktopChildSiteBridge) that is resized
// in lockstep with the frame, but its composition runs a tick or two behind; in the gap the
// parent's redirection surface shows through, freshly erased with that white brush. Nothing
// composition-side can help: a compositor target on the HWND and the system-backdrop slot both
// sit *below* the redirection surface (both were tried, both lost -- the stripes stayed white).
// So the fix is the classic Win32 one: own the erase. The class brush is taken away,
// WM_ERASEBKGND is answered by painting the backdrop into the given DC, and WM_SIZE paints it
// synchronously inside the resize loop -- before the framework's own handler runs, before DWM
// shows the new frame. The surface then never holds anything but the backdrop, and the gap
// continues the picture instead of flashing.
//
// GDI on purpose, which is why NOGDI is lifted for this one file: the redirection surface is a
// GDI surface, and painting it with anything else would mean another compositor -- the thing that
// just proved to be on the wrong side of it.
//
// The projection comes first, and with it every standard header it needs: wxl's own headers carry
// the wxl.core import, and a standard header included after that import is one the compiler has
// already seen through the std module.
#undef NOGDI
#include "platform.h"

#include <commctrl.h>
#include <shellapi.h>
#include <wincodec.h>

#include <map>
#include <vector>

#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/base.h>

#include <microsoft.ui.xaml.window.h>

#include "Object.impl.h"
#include "WindowNative.h"
#include <wxl/Microsoft.UI.Windowing.h>
#include <wxl/Microsoft.UI.Windowing.impl.h>
#include <wxl/Microsoft.UI.Xaml.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>
#include "impl/window_helpers.h"

// Imports last: they bring the standard library as a module, and a plain
// header after one of them is what MSVC will not take.
import wxl.core;

namespace wxl {

namespace {

namespace windowing = winrt::Microsoft::UI::Windowing;

using drop_handler = std::function<void(std::vector<std::wstring> const&)>;

// Three states, not two. Maximized apart from normal, because otherwise a
// user who always works maximized gets a restored window every launch;
// full-screen apart from both, because Windows does not know it -- it is a
// WinUI presenter, or a borderless cover the composited window puts up itself.
constexpr std::u16string_view normal_state = u"normal";
constexpr std::u16string_view maximized_state = u"maximized";
constexpr std::u16string_view full_screen_state = u"fullscreen";

// The window's HWND, which is where Win32 keeps everything below.
//
// Through IWindowNative, the interface Window itself implements, rather than
// through the WindowId that AppWindow hands out: the call that turns one of
// those into a handle lives in Microsoft.UI.Interop.h, and that header wants
// an ABI header its own package does not carry.
HWND handle_of(winrt::Microsoft::UI::Xaml::Window const& window) {
    HWND hwnd{};
    if (auto const native = window.try_as<::IWindowNative>()) {
        native->get_WindowHandle(&hwnd);
    }
    return hwnd;
}

/// The paths inside an HDROP.
std::vector<std::wstring> paths_of(HDROP drop) {
    std::vector<std::wstring> paths;
    UINT const count = ::DragQueryFileW(drop, 0xFFFFFFFF, nullptr, 0);
    paths.reserve(count);

    for (UINT index = 0; index < count; ++index) {
        // Asked for twice on purpose: once for the length, once for the text.
        // The length comes back without the terminator, which is why the
        // buffer is one longer than the string it will hold.
        UINT const length = ::DragQueryFileW(drop, index, nullptr, 0);
        if (length == 0) {
            continue;
        }
        std::wstring path(length, L'\0');
        if (::DragQueryFileW(drop, index, path.data(), length + 1) != 0) {
            paths.push_back(std::move(path));
        }
    }
    return paths;
}

/// The image as a plain 32-bit DIB, decoded once at its natural size.
HBITMAP load_image(const wchar_t* path, SIZE& size) {
    winrt::com_ptr<IWICImagingFactory> wic;
    winrt::check_hresult(::CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
                                            IID_PPV_ARGS(wic.put())));

    winrt::com_ptr<IWICBitmapDecoder> decoder;
    winrt::check_hresult(wic->CreateDecoderFromFilename(
        path, nullptr, GENERIC_READ, WICDecodeMetadataCacheOnLoad, decoder.put()));
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
    info.bmiHeader = {sizeof(BITMAPINFOHEADER),
                      static_cast<LONG>(width),
                      -static_cast<LONG>(height),
                      1,
                      32,
                      BI_RGB};
    void* bits = nullptr;
    HBITMAP const bitmap = ::CreateDIBSection(nullptr, &info, DIB_RGB_COLORS, &bits, nullptr, 0);
    if (!bitmap) winrt::throw_last_error();

    winrt::check_hresult(
        converter->CopyPixels(nullptr, width * 4, width * height * 4, static_cast<BYTE*>(bits)));
    size = {static_cast<LONG>(width), static_cast<LONG>(height)};
    return bitmap;
}

}  // namespace

/// Everything wxl keeps of one window, by its handle.
struct WindowNative::native_state {
    /// The rectangle the window returns to, as remembered here rather than by Windows. WinUI's
    /// full-screen presenter overwrites rcNormalPosition with the size of the display, and after
    /// that Win32 no longer knows where the window came from -- so a window closed full-screen
    /// would be remembered as "fullscreen 0 0 1920 1080" and, on leaving full screen next time,
    /// hand the user a window the size of their monitor.
    RECT restore{};
    bool restore_known = false;

    /// What the window's surface is painted with: a solid color, or an image at its natural
    /// size, stretched at paint time; nothing until a backdrop is set. `painted` and `fine` say
    /// what the surface holds right now -- the size it was painted for and whether with the slow
    /// filtered stretch -- so nothing paints twice for one size.
    bool backdrop = false;
    HBITMAP image = nullptr;
    SIZE image_size{};
    COLORREF color = 0;
    SIZE painted{};
    bool fine = false;

    /// The handler of the files dropped on the window; empty until acceptFileDrops.
    drop_handler drops;
};

/// The one subclass procedure: the backdrop's paints, the dropped files, and the window's going.
struct WindowNative::subclass {
    /// One is enough: the procedure below is the only one this store installs, and the pair
    /// (procedure, id) is what identifies it.
    static constexpr UINT_PTR id = 1;

    /// The store, keyed by handle. Leaked deliberately: an entry lives exactly as long as its
    /// window, and the process is on its way out anyway when the static would go.
    static std::map<HWND, native_state>& states() {
        static auto* const kept = new std::map<HWND, native_state>;
        return *kept;
    }

    /// Cover, pinned to the top centre: proportions kept, the excess leaves through the sides --
    /// the same UniformToFill the XAML splash uses, so the surface and the island agree about
    /// every pixel they both show.
    ///
    /// Two speeds on purpose. HALFTONE filters per pixel on the CPU, and a full-window pass of it
    /// on every WM_SIZE is exactly what makes a resize drag feel heavy -- so the drag paints
    /// COLORONCOLOR, a plain scaled copy, and the filtered pass runs once, when the size has
    /// settled.
    static void paint(HWND hwnd, HDC dc, native_state& state, bool fine) {
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

        double const scale = std::max(static_cast<double>(client.right) / state.image_size.cx,
                                      static_cast<double>(client.bottom) / state.image_size.cy);
        int const width = static_cast<int>(state.image_size.cx * scale + 0.5);
        int const height = static_cast<int>(state.image_size.cy * scale + 0.5);

        HDC const memory = ::CreateCompatibleDC(dc);
        HGDIOBJ const previous = ::SelectObject(memory, state.image);
        ::SetStretchBltMode(dc, fine ? HALFTONE : COLORONCOLOR);
        ::SetBrushOrgEx(dc, 0, 0, nullptr);
        ::StretchBlt(dc, (client.right - width) / 2, 0, width, height, memory, 0, 0,
                     state.image_size.cx, state.image_size.cy, SRCCOPY);
        ::SelectObject(memory, previous);
        ::DeleteDC(memory);
    }

    /// Paints once right away: the surface must be right from the first frame, not from the
    /// first resize.
    static void repaint_now(HWND hwnd, native_state& state) {
        HDC const dc = ::GetDC(hwnd);
        paint(hwnd, dc, state, true);
        ::ReleaseDC(hwnd, dc);
    }

    /// The backdrop is repainted right here, synchronously inside the resize loop, before the
    /// framework's own WM_SIZE work and before DWM shows the frame at its new size -- the
    /// zero-latency path a flicker-free Win32 application uses. Exactly once per size: the erase
    /// that follows sees the surface already current and only says so, and the filtered pass
    /// waits for WM_EXITSIZEMOVE.
    ///
    /// noexcept: an exception must not cross a Win32 callback -- unwinding through the frames of
    /// user32 is not supported, and on x64 a call from the kernel may lose it -- so a failure
    /// inside is std::terminate, which names it.
    static LRESULT CALLBACK procedure(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam,
                                      UINT_PTR, DWORD_PTR data) noexcept {
        auto& state = *reinterpret_cast<native_state*>(data);
        switch (message) {
            case WM_ERASEBKGND:
                if (state.backdrop) {
                    RECT client{};
                    ::GetClientRect(hwnd, &client);
                    if (client.right != state.painted.cx || client.bottom != state.painted.cy) {
                        paint(hwnd, reinterpret_cast<HDC>(wparam), state, true);
                    }
                    return 1;
                }
                break;

            case WM_SIZE:
                if (state.backdrop && wparam != SIZE_MINIMIZED) {
                    HDC const dc = ::GetDC(hwnd);
                    // Maximize is one event, not a drag: it can afford the
                    // filtered pass, and no WM_EXITSIZEMOVE follows to run it.
                    paint(hwnd, dc, state, wparam == SIZE_MAXIMIZED);
                    ::ReleaseDC(hwnd, dc);
                }
                break;

            case WM_EXITSIZEMOVE:
                if (state.backdrop && !state.fine) repaint_now(hwnd, state);
                break;

            case WM_DROPFILES:
                if (state.drops) {
                    auto const drop = reinterpret_cast<HDROP>(wparam);
                    auto const paths = paths_of(drop);
                    // Before the handler, not after: the handler may open a document,
                    // put up a dialog or take a while, and the shell's memory has no
                    // business waiting for it.
                    ::DragFinish(drop);
                    if (!paths.empty()) state.drops(paths);
                    return 0;
                }
                break;

            case WM_NCDESTROY:
                // The window is going, and with it the reason to hold anything of it. A
                // subclass that outlived its window would be a dangling procedure on a handle
                // the system is free to hand out again.
                ::RemoveWindowSubclass(hwnd, &procedure, id);
                if (state.image) ::DeleteObject(state.image);
                states().erase(hwnd);
                break;

            default:
                break;
        }

        return ::DefSubclassProc(hwnd, message, wparam, lparam);
    }
};

WindowNative::native_state* WindowNative::state_of(HWND__* hwnd) {
    if (!hwnd) return nullptr;

    auto& all = subclass::states();
    if (auto const found = all.find(hwnd); found != all.end()) return &found->second;

    native_state& state = all[hwnd];
    if (!::SetWindowSubclass(hwnd, &subclass::procedure, subclass::id,
                             reinterpret_cast<DWORD_PTR>(&state))) {
        all.erase(hwnd);
        return nullptr;
    }
    return &state;
}

HWND__* WindowNative::handle(Window const& window) {
    return handle_of(*Object::Impl::get_typed<Window>(window));
}

hstring WindowNative::placement(Window const& window) {
    winrt::Microsoft::UI::Xaml::Window const& real = *Object::Impl::get_typed<Window>(window);
    HWND const hwnd = handle_of(real);
    if (!hwnd) {
        return {};
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(hwnd, &placement)) {
        return {};
    }

    auto const presenter = real.AppWindow().Presenter();
    bool const full_screen =
        presenter && presenter.Kind() == windowing::AppWindowPresenterKind::FullScreen;

    // A minimized window is not remembered as minimized: a user who closed
    // it from the taskbar expects a window next time, not an icon.
    impl::placement_state const state = full_screen ? impl::placement_state::full_screen
                                        : placement.showCmd == SW_SHOWMAXIMIZED
                                            ? impl::placement_state::maximized
                                            : impl::placement_state::normal;

    // Full screen is the one state whose rcNormalPosition is not to be
    // believed; every other one refreshes what we believe instead -- of a window
    // that has an entry: reading makes none.
    RECT rc = placement.rcNormalPosition;
    auto& all = subclass::states();
    if (auto const found = all.find(hwnd); found != all.end()) {
        native_state& kept = found->second;
        if (!full_screen) {
            kept.restore = rc;
            kept.restore_known = true;
        } else if (kept.restore_known) {
            rc = kept.restore;
        }
    }

    std::u16string text{impl::name_of(state)};
    for (long const value : {rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top}) {
        text += u' ';
        wxl::core::append_number(text, value);
    }
    return hstring{text};
}

void WindowNative::setBackdropColor(Window const& window, Color color) {
    HWND const hwnd = handle(window);
    native_state* const state = state_of(hwnd);
    if (!state) return;

    // The class brush goes away so nothing fills the grown surface white on
    // the kernel's side of WM_SIZE. If a fill still races a DWM frame, it
    // now shows a moment of the previous paint -- the same picture, one
    // size stale -- instead of a white stripe.
    ::SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, 0);

    if (state->image) ::DeleteObject(state->image);
    state->image = nullptr;
    state->color = RGB(color.R, color.G, color.B);
    state->backdrop = true;
    subclass::repaint_now(hwnd, *state);
}

void WindowNative::setBackdropImage(Window const& window, const wchar_t* imagePath) {
    SIZE size{};
    HBITMAP const image = load_image(imagePath, size);

    HWND const hwnd = handle(window);
    native_state* const state = state_of(hwnd);
    if (!state) {
        ::DeleteObject(image);
        return;
    }

    ::SetClassLongPtrW(hwnd, GCLP_HBRBACKGROUND, 0);

    if (state->image) ::DeleteObject(state->image);
    state->image = image;
    state->image_size = size;
    state->backdrop = true;
    subclass::repaint_now(hwnd, *state);
}

bool WindowNative::acceptFileDrops(Window const& window, drop_handler handler) {
    HWND const hwnd = handle(window);
    native_state* const state = state_of(hwnd);
    if (!state) {
        return false;
    }

    // Registering twice on the same handle is an application changing its mind about the
    // handler, not a second subscriber: the handler is replaced, and the window is told once.
    bool const known = static_cast<bool>(state->drops);
    state->drops = std::move(handler);
    if (!known) ::DragAcceptFiles(hwnd, TRUE);
    return true;
}

void WindowNative::makeModal(Window const& dialog, Window const& owner) {
    // The owner, the one piece with no WinUI API: a modal window needs one, and
    // OverlappedPresenter.IsModal is what disables it. The Win32 owner
    // (GWLP_HWNDPARENT) is also what keeps an owned window off the taskbar.
    HWND const dialogHwnd = handle(dialog);
    HWND const ownerHwnd = handle(owner);
    if (!dialogHwnd || !ownerHwnd) return;
    ::SetWindowLongPtrW(dialogHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));

    AppWindow const appWindow = dialog.appWindow();

    // Off the taskbar and out of Alt+Tab. Being owned already does most of this;
    // asking outright covers the switcher too.
    appWindow.isShownInSwitchers(false);

    // A presenter made for a dialog, with the modal property set before it is
    // given to the window: IsModal set on the presenter a window already has
    // changes the property and nothing else -- the owner stays enabled -- while
    // a presenter handed over with it set disables the owner for as long as the
    // dialog is open.
    OverlappedPresenter const presenter = OverlappedPresenter::createForDialog();
    presenter.isModal(true);
    presenter.isMinimizable(false);
    presenter.isMaximizable(false);
    appWindow.setPresenter(presenter);
}

namespace impl {

std::optional<placement_geometry> parse_placement(std::u16string_view text) {
    auto const word = [&text]() -> std::u16string_view {
        while (!text.empty() && text.front() == u' ') {
            text.remove_prefix(1);
        }
        auto const end = text.find(u' ');
        auto const taken = text.substr(0, end);
        text.remove_prefix(end == std::u16string_view::npos ? text.size() : end);
        return taken;
    };

    placement_geometry result{};
    auto const state = word();
    if (state == normal_state) {
        result.state = placement_state::normal;
    } else if (state == maximized_state) {
        result.state = placement_state::maximized;
    } else if (state == full_screen_state) {
        result.state = placement_state::full_screen;
    } else {
        return std::nullopt;
    }

    long numbers[4]{};
    for (long& value : numbers) {
        // The whole word has to be the number, which is what try_parse asks of
        // it -- and it asks it of a view, so nothing is copied to get a
        // terminator. It reads no locale either, which matters for a value that
        // was written to a settings file on some other machine.
        // Read on the GUI thread, so a buffer, if a word ever needs one,
        // comes from that thread's pool.
        if (!wxl::core::try_parse<wxl::core::sta_allocator>(word(), value)) {
            return std::nullopt;
        }
    }

    // A side of zero or less is not a rectangle, it is rubbish.
    if (numbers[2] <= 0 || numbers[3] <= 0) {
        return std::nullopt;
    }

    result.restore = {numbers[0], numbers[1], numbers[0] + numbers[2], numbers[1] + numbers[3]};
    return result;
}

std::u16string_view name_of(placement_state state) {
    switch (state) {
        case placement_state::maximized:
            return maximized_state;
        case placement_state::full_screen:
            return full_screen_state;
        case placement_state::normal:
            break;
    }
    return normal_state;
}

RECT fit_placement_to_displays(RECT const& wanted) {
    winrt::Windows::Graphics::RectInt32 const asked{
        wanted.left, wanted.top, wanted.right - wanted.left, wanted.bottom - wanted.top};
    auto const display =
        windowing::DisplayArea::GetFromRect(asked, windowing::DisplayAreaFallback::Nearest);
    if (!display) {
        return wanted;
    }

    auto const work = display.WorkArea();
    RECT const area{work.X, work.Y, work.X + work.Width, work.Y + work.Height};

    RECT overlap{};
    if (::IntersectRect(&overlap, &wanted, &area)) {
        return wanted;
    }

    long const width = std::min<long>(asked.Width, work.Width);
    long const height = std::min<long>(asked.Height, work.Height);
    long const left = area.left + (work.Width - width) / 2;
    long const top = area.top + (work.Height - height) / 2;
    return {left, top, left + width, top + height};
}

void window_placement_setter::set(winrt::Microsoft::UI::Xaml::Window const& window,
                                  hstring_param const& placement_text) {
    std::u16string_view const text = placement_text.text().view();
    auto const wanted = parse_placement(text);
    if (!wanted) {
        return;  // nothing said: the window stays as it was created
    }

    HWND const hwnd = handle_of(window);
    if (!hwnd) {
        return;
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(hwnd, &placement)) {
        return;
    }

    // rcNormalPosition is where the window returns to from maximized, and it
    // is already in work-area coordinates -- corrected for the taskbar. That
    // is the rectangle worth remembering: a window closed maximized would
    // report the size of the screen, and leaving the maximized state next
    // time would hand the user a window as large as their monitor.
    placement.rcNormalPosition = fit_placement_to_displays(wanted->restore);
    placement.showCmd =
        wanted->state == placement_state::maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    ::SetWindowPlacement(hwnd, &placement);

    // Remembered before the presenter is asked for anything: a window that
    // starts full-screen has to know where to come back to, and this is the
    // only moment anybody says.
    if (WindowNative::native_state* const kept = WindowNative::state_of(hwnd)) {
        kept->restore = placement.rcNormalPosition;
        kept->restore_known = true;
    }

    // Full screen after the placement, not instead of it: the presenter
    // covers the display, but the window still has somewhere to come back
    // to, and that somewhere has just been put right.
    if (wanted->state == placement_state::full_screen) {
        window.AppWindow().SetPresenter(windowing::AppWindowPresenterKind::FullScreen);
    }
}

}  // namespace impl

}  // namespace wxl
