#include <windows.h>

#include "impl/app_icon.h"

namespace wxl::impl {

namespace {

// Resource id 1: the icon the shell shows for the executable, the one
// wxl_target_icon() writes into the generated script, and the one
// wxl::TrayIcon falls back to. Asked for at the size it will be shown at --
// the title bar takes the small one and Alt+Tab the large one, and an .ico
// carries both -- so nothing is scaled from the wrong size.
HICON iconOfThisModule(int width, int height) noexcept {
    return static_cast<HICON>(::LoadImageW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1),
                                           IMAGE_ICON, width, height, LR_SHARED));
}

// Both places a window's icon can come from. WM_GETICON answers for the window
// itself and says nothing about the class behind it, and a window drawn with
// the class icon is a window with an icon -- ours are exactly that.
bool hasIcon(HWND window) noexcept {
    if (::SendMessageW(window, WM_GETICON, ICON_SMALL, 0)) return true;
    if (::SendMessageW(window, WM_GETICON, ICON_BIG, 0)) return true;

    return ::GetClassLongPtrW(window, GCLP_HICONSM) || ::GetClassLongPtrW(window, GCLP_HICON);
}

struct Icons {
    HICON large;
    HICON small;
};

}  // namespace

void apply_application_icon() noexcept {
    Icons const icons{
        iconOfThisModule(::GetSystemMetrics(SM_CXICON), ::GetSystemMetrics(SM_CYICON)),
        iconOfThisModule(::GetSystemMetrics(SM_CXSMICON), ::GetSystemMetrics(SM_CYSMICON)),
    };

    // No icon resource in this executable: the application either ships none or
    // sets its own, and either way there is nothing here to hand out.
    if (!icons.large && !icons.small) return;

    // Only the thread's own top-level windows, which is every window an
    // application put up from wxl_launched: EnumThreadWindows skips child
    // windows, and with them the XAML island's own hosts. Invisible ones are
    // skipped too -- a window nobody has shown has nowhere to show an icon,
    // and the ones WinUI keeps for itself are exactly that.
    ::EnumThreadWindows(
        ::GetCurrentThreadId(),
        [](HWND window, LPARAM state) noexcept -> BOOL {
            Icons const& icons = *reinterpret_cast<Icons const*>(state);

            if (::IsWindowVisible(window) && !hasIcon(window)) {
                if (icons.large)
                    ::SendMessageW(window, WM_SETICON, ICON_BIG,
                                   reinterpret_cast<LPARAM>(icons.large));
                if (icons.small)
                    ::SendMessageW(window, WM_SETICON, ICON_SMALL,
                                   reinterpret_cast<LPARAM>(icons.small));
            }
            return TRUE;
        },
        reinterpret_cast<LPARAM>(&icons));
}

}  // namespace wxl::impl
