// makeModalDialog, the winrt side: the Win32 owner, then WinUI's modal and
// off-taskbar properties.
//
// The Windows and projection headers come first, and with them the standard
// library they pull in: the wxl headers below carry the wxl.core import, and a
// standard header after that import is one MSVC has already seen through the std
// module. windows.h is explicit -- SetWindowLongPtrW and GWLP_HWNDPARENT are
// ours to reach for, and the projection headers do not pull them in.
#include <windows.h>

#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "Object.impl.h"
#include "generated/Microsoft.UI.Windowing.h"
#include "generated/Microsoft.UI.Windowing.impl.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "WindowHandle.h"
#include "WindowModal.h"

namespace wxl {

void makeModalDialog(Window const& dialog, Window const& owner) {
    // The owner, the one piece with no WinUI API: a modal window needs one, and
    // OverlappedPresenter.IsModal is what disables it. The Win32 owner
    // (GWLP_HWNDPARENT) is also what keeps an owned window off the taskbar.
    HWND const dialogHwnd = window_handle(dialog);
    HWND const ownerHwnd = window_handle(owner);
    if (!dialogHwnd || !ownerHwnd) return;
    ::SetWindowLongPtrW(dialogHwnd, GWLP_HWNDPARENT, reinterpret_cast<LONG_PTR>(ownerHwnd));

    AppWindow const appWindow = dialog.appWindow();

    // Off the taskbar and out of Alt+Tab. Being owned already does most of this;
    // asking outright covers the switcher too.
    appWindow.isShownInSwitchers(false);

    // The default presenter of an ordinary window is an OverlappedPresenter;
    // recover it and make the window modal and fixed.
    if (OverlappedPresenter const presenter =
            appWindow.presenter().try_as<OverlappedPresenter>()) {
        presenter.isModal(true);
        presenter.isMinimizable(false);
        presenter.isMaximizable(false);
    }
}

}  // namespace wxl
