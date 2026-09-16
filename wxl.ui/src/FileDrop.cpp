// Files dropped on the window, taken the short way the shell has offered
// since Windows 3.1.
//
// The projection and the Windows headers come first, and with them every
// standard header they need: wxl's own headers carry the wxl.core import, and
// a standard header included after that import is one the compiler has already
// seen through the std module.
#include <winrt/Microsoft.UI.Xaml.h>

#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <map>
#include <vector>

#include "FileDrop.h"
#include "WindowHandle.h"
#include "generated/Microsoft.UI.Xaml.h"

namespace wxl {
namespace {

using drop_handler = std::function<void(std::vector<std::wstring> const&)>;

/// One handler per window, kept for as long as the window lives.
///
/// Registering twice on the same handle is an application changing its mind
/// about the handler, not a second subscriber: the entry is replaced and the
/// subclass below stays where it is.
std::map<HWND, drop_handler>& handlers() {
    static std::map<HWND, drop_handler> registered;
    return registered;
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

/// Subclass id. One is enough: the subclass procedure below is the only one
/// wxl installs, and the pair (procedure, id) is what identifies it.
constexpr UINT_PTR kSubclassId = 1;

LRESULT CALLBACK dropped(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam, UINT_PTR,
                         DWORD_PTR) {
    switch (message) {
        case WM_DROPFILES: {
            auto const drop = reinterpret_cast<HDROP>(wparam);
            auto const paths = paths_of(drop);
            // Before the handler, not after: the handler may open a book,
            // put up a dialog or take a while, and the shell's memory has no
            // business waiting for it.
            ::DragFinish(drop);

            auto& registered = handlers();
            if (auto const found = registered.find(hwnd);
                found != registered.end() && found->second && !paths.empty()) {
                found->second(paths);
            }
            return 0;
        }

        case WM_NCDESTROY:
            // The window is going, and with it the reason to hold either. A
            // subclass that outlived its window would be a dangling procedure
            // on a handle the system is free to hand out again.
            handlers().erase(hwnd);
            ::RemoveWindowSubclass(hwnd, dropped, kSubclassId);
            break;

        default:
            break;
    }

    return ::DefSubclassProc(hwnd, message, wparam, lparam);
}

}  // namespace

bool accept_file_drops(Window const& window, drop_handler handler) {
    HWND const hwnd = window_handle(window);
    if (!hwnd) {
        return false;
    }

    auto& registered = handlers();
    bool const known = registered.find(hwnd) != registered.end();
    registered[hwnd] = std::move(handler);
    if (known) {
        return true;   // already subclassed; only the handler is new
    }

    if (!::SetWindowSubclass(hwnd, dropped, kSubclassId, 0)) {
        registered.erase(hwnd);
        return false;
    }

    ::DragAcceptFiles(hwnd, TRUE);
    return true;
}

}  // namespace wxl
