// Where the window stood last time, and how to put it back there.
//
// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header
// included after that import is one the compiler has already seen through
// the std module.
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include <microsoft.ui.xaml.window.h>

#include "Object.impl.h"
#include "WindowHandle.h"
#include "WindowPlacement.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "impl/window_placement.h"

// Imports last: they bring the standard library as a module, and a plain
// header after one of them is what MSVC will not take.
import wxl.core;

namespace wxl {
namespace {

namespace windowing = winrt::Microsoft::UI::Windowing;

// Three states, not two. Maximized apart from normal, because otherwise a
// reader who always works maximized gets a restored window every launch;
// full-screen apart from both, because Windows does not know it -- it is a
// WinUI presenter, or a borderless cover the composited window puts up itself.
constexpr std::wstring_view normal_state = L"normal";
constexpr std::wstring_view maximized_state = L"maximized";
constexpr std::wstring_view full_screen_state = L"fullscreen";

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

/// The rectangle a window returns to, as remembered here rather than by
/// Windows.
///
/// WinUI's full-screen presenter overwrites rcNormalPosition with the size of
/// the display, and after that Win32 no longer knows where the window came
/// from -- so a window closed full-screen would be remembered as
/// "fullscreen 0 0 1920 1080" and, on leaving full screen next time, hand the
/// reader a window the size of their monitor. Which is the very thing this
/// whole property exists to avoid, one presenter further along.
///
/// Keyed by HWND, because an application's windows are not necessarily one.
/// Nothing is ever removed: an entry is two rectangles' worth of bytes, and
/// a window that closed is a window that may reopen.
std::map<HWND, RECT>& remembered_restores() {
    static std::map<HWND, RECT> rectangles;
    return rectangles;
}

}  // namespace

namespace impl {

std::optional<placement_geometry> parse_placement(std::wstring_view text) {
    auto const word = [&text]() -> std::wstring_view {
        while (!text.empty() && text.front() == L' ') {
            text.remove_prefix(1);
        }
        auto const end = text.find(L' ');
        auto const taken = text.substr(0, end);
        text.remove_prefix(end == std::wstring_view::npos ? text.size() : end);
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

std::wstring_view name_of(placement_state state) {
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
    winrt::Windows::Graphics::RectInt32 const asked{wanted.left, wanted.top,
                                                    wanted.right - wanted.left,
                                                    wanted.bottom - wanted.top};
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

void set_window_placement(winrt::Microsoft::UI::Xaml::Window const& window,
                          string_param placement_text) {
    std::wstring_view const text = placement_text.wide();
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
    // time would hand the reader a window as large as their monitor.
    placement.rcNormalPosition = fit_placement_to_displays(wanted->restore);
    placement.showCmd =
        wanted->state == placement_state::maximized ? SW_SHOWMAXIMIZED : SW_SHOWNORMAL;
    ::SetWindowPlacement(hwnd, &placement);

    // Remembered before the presenter is asked for anything: a window that
    // starts full-screen has to know where to come back to, and this is the
    // only moment anybody says.
    remembered_restores()[hwnd] = placement.rcNormalPosition;

    // Full screen after the placement, not instead of it: the presenter
    // covers the display, but the window still has somewhere to come back
    // to, and that somewhere has just been put right.
    if (wanted->state == placement_state::full_screen) {
        window.AppWindow().SetPresenter(windowing::AppWindowPresenterKind::FullScreen);
    }
}

std::wstring get_window_placement(winrt::Microsoft::UI::Xaml::Window const& window) {
    HWND const hwnd = handle_of(window);
    if (!hwnd) {
        return {};
    }

    WINDOWPLACEMENT placement{};
    placement.length = sizeof(placement);
    if (!::GetWindowPlacement(hwnd, &placement)) {
        return {};
    }

    auto const presenter = window.AppWindow().Presenter();
    bool const full_screen =
        presenter && presenter.Kind() == windowing::AppWindowPresenterKind::FullScreen;

    // A minimized window is not remembered as minimized: a reader who closed
    // it from the taskbar expects a window next time, not an icon.
    placement_state const state = full_screen ? placement_state::full_screen
                                  : placement.showCmd == SW_SHOWMAXIMIZED
                                      ? placement_state::maximized
                                      : placement_state::normal;

    // Full screen is the one state whose rcNormalPosition is not to be
    // believed; every other one refreshes what we believe instead.
    RECT rc = placement.rcNormalPosition;
    if (full_screen) {
        auto const& remembered = remembered_restores();
        if (auto const found = remembered.find(hwnd); found != remembered.end()) {
            rc = found->second;
        }
    } else {
        remembered_restores()[hwnd] = rc;
    }

    std::wstring text{name_of(state)};
    for (long const value : {rc.left, rc.top, rc.right - rc.left, rc.bottom - rc.top}) {
        text += L' ';
        wxl::core::append_number(text, value);
    }
    return text;
}

}  // namespace impl

// Not in a file of its own: the handle and the placement are the same
// question -- what Win32 still owns of a WinUI window -- and handle_of above
// is the answer to both.
HWND__* window_handle(Window const& window) {
    return handle_of(*Object::Impl::get_typed<Window>(window));
}

wstring window_placement(Window const& window) {
    auto const text = impl::get_window_placement(*Object::Impl::get_typed<Window>(window));

    // wchar_t and char16_t are the same 16-bit code unit on Windows and
    // differ only in type -- the same crossing impl/conversions.h makes for
    // every string that comes back from WinRT.
    return wstring{reinterpret_cast<char16_t const*>(text.c_str()), text.size()};
}

}  // namespace wxl
