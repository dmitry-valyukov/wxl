// The notification area, taken the only way the shell offers it: a window for
// the shell to send its messages to, and Shell_NotifyIcon to say what to show.
//
// That window is a WinUI one. It has to be: the menu the shell asks us to raise
// is a WinUI MenuFlyout, and a flyout needs a XamlRoot to live in, which only a
// window with XAML content can give. And it is the *same* window that receives
// the shell's messages -- a second, XAML-less window just for the messages would
// save nothing, since raising that menu is most of what a tray icon is for. So
// one window does both: WinUI owns it and lends the flyout its XamlRoot, and a
// subclass catches the shell's messages ahead of WinUI's own handling, the way
// the console host is driven from Trayed.
//
// Windows and projection headers first, and with them the standard library they
// pull in: wxl's own headers carry the wxl.core import, and a standard header
// after that import is one MSVC has already seen through the std module.
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>
#include <windowsx.h>

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include <algorithm>
#include <iterator>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "Collection.impl.h"
#include "Object.impl.h"
#include "events.h"
#include "generated/Microsoft.UI.Windowing.h"
#include "generated/Microsoft.UI.Windowing.impl.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "WindowHandle.h"
#include "TrayIcon.h"

namespace wxl {
namespace {

/// The shell's message to us: clicks, hovers, menu requests. One message for
/// all of them, with the event in the low word of lParam.
constexpr UINT kIconMessage = WM_APP + 1;

/// post()'s message. The lParam is a std::function this window owns from the
/// moment the post succeeds.
constexpr UINT kRunMessage = WM_APP + 2;

/// The icon's id within this window. One window, one icon, so it never varies.
constexpr UINT kIconId = 1;

/// The subclass id our window procedure is installed under. One per window.
constexpr UINT_PTR kSubclassId = 1;

/// Explorer's "I have restarted, add your icons again". Registered once; the
/// value is the same for every window in the system.
UINT taskbarCreated() {
    static UINT const message = ::RegisterWindowMessageW(L"TaskbarCreated");
    return message;
}

/// The application's own icon, or the system's generic one. Never null: an icon
/// that failed to load leaves an invisible but clickable gap in the tray.
HICON applicationIcon() {
    if (HICON const own = ::LoadIconW(::GetModuleHandleW(nullptr), MAKEINTRESOURCEW(1)))
        return own;

    // IDI_APPLICATION is spelled for the character set the translation unit was
    // built for, and this one is not built for UNICODE. What it holds is an
    // integer id encoded as a pointer, and that is the same either way.
    return ::LoadIconW(nullptr, reinterpret_cast<LPCWSTR>(IDI_APPLICATION));
}

/// Builds the flyout rows from the items, into a collection that is either the
/// flyout's own or a submenu's -- so submenus are the same call, one level down.
void fillItems(Collection<MenuFlyoutItemBase> const& into,
               std::vector<TrayIcon::Item> const& items) {
    for (TrayIcon::Item const& item : items) {
        if (item.text.empty()) {
            into.append(MenuFlyoutSeparator{});
            continue;
        }

        if (!item.children.empty()) {
            MenuFlyoutSubItem sub;
            sub.text(item.text);
            sub.isEnabled(item.enabled);
            fillItems(sub.items(), item.children);
            into.append(sub);
            continue;
        }

        // The handler is copied into the Click closure -- the flyout is thrown
        // away and rebuilt on every open, so nothing here outlives one showing.
        std::function<void()> const onClick = item.onClick;
        auto const clicked = [onClick](Object const&, RoutedEventArgs&) {
            if (onClick) onClick();
        };

        if (item.checked) {
            ToggleMenuFlyoutItem row;
            row.text(item.text);
            row.isChecked(true);
            row.isEnabled(item.enabled);
            row.add_onClick(clicked);
            into.append(row);
        } else {
            MenuFlyoutItem row;
            row.text(item.text);
            row.isEnabled(item.enabled);
            row.add_onClick(clicked);
            into.append(row);
        }
    }
}

}  // namespace

// The WinUI wrappers behind the icon's window: the window itself, the content
// that lends the flyout a XamlRoot, and the flyout currently up.
class TrayIcon::Impl {
public:
    Window window;
    Grid anchor;
    // A default MenuFlyout is empty and harmless; replaced on every open.
    MenuFlyout flyout;

    Impl() {
        window.content(anchor);
        // The marker the foreground tracking knows this window by: it is a WinUI
        // window like the application's own now, so the window class no longer
        // tells them apart -- the title does. Never seen, the window being
        // hidden and transparent.
        window.title(L"wxl.TrayIcon");

        AppWindow const appWindow = window.appWindow();
        appWindow.isShownInSwitchers(false);
        if (OverlappedPresenter const presenter =
                appWindow.presenter().try_as<OverlappedPresenter>()) {
            presenter.setBorderAndTitleBar(false, false);
            presenter.isAlwaysOnTop(true);
        }
        appWindow.resize(SizeInt32{1, 1});
    }
};

/// The window procedure's way into a TrayIcon: a friend, so the icon itself need
/// not put its message handling in the header.
struct TrayIconAccess {
    /// NIM_ADD plus the version handshake, in one place because Explorer's
    /// restart calls for exactly the same thing again.
    static bool addIcon(TrayIcon& icon) {
        NOTIFYICONDATAW data{};
        data.cbSize = sizeof data;
        data.hWnd = reinterpret_cast<HWND>(icon.window_);
        data.uID = kIconId;
        data.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP | NIF_SHOWTIP;
        data.uCallbackMessage = kIconMessage;
        data.hIcon = reinterpret_cast<HICON>(icon.icon_);

        // szTip is the shell's buffer and its size is the shell's rule; a tooltip
        // longer than it holds is cut rather than refused.
        std::size_t const fits = std::min(icon.tooltip_.size(), std::size(data.szTip) - 1);
        icon.tooltip_.copy(data.szTip, fits);
        data.szTip[fits] = L'\0';

        if (!::Shell_NotifyIconW(NIM_ADD, &data)) return false;

        // Version 4 is what makes the shell send WM_CONTEXTMENU and NIN_SELECT
        // with the anchor point in wParam, instead of leaving us to ask where the
        // cursor is and hope it has not moved.
        data.uVersion = NOTIFYICON_VERSION_4;
        ::Shell_NotifyIconW(NIM_SETVERSION, &data);
        return true;
    }

    /// Raises the menu as a WinUI flyout at the point the shell reported. Built
    /// fresh every time, so a checkmark standing for something the application
    /// owns is always current; each item runs its own onClick, and the flyout
    /// dismisses itself -- nothing is returned.
    static void showMenu(TrayIcon& icon, POINT at) {
        if (!icon.build_ || !icon.impl_) return;

        std::vector<TrayIcon::Item> const items = icon.build_();
        if (items.empty()) return;

        TrayIcon::Impl& impl = *icon.impl_;
        AppWindow const appWindow = impl.window.appWindow();

        // The window rides to the cursor to anchor the flyout there, then is made
        // foreground both ways -- activate shows it, SetForegroundWindow makes it
        // the window the flyout can take input through and dismiss with.
        appWindow.move(PointInt32{at.x, at.y});
        impl.window.activate();
        ::SetForegroundWindow(reinterpret_cast<HWND>(icon.window_));

        impl.flyout = MenuFlyout{};
        fillItems(impl.flyout.items(), items);
        // The window has done its job once the flyout is gone: hide it again.
        impl.flyout.add_onClosed(
            [appWindow](Object const&, Object const&) { appWindow.hide(); });
        impl.flyout.showAt(impl.anchor);
    }

    static LRESULT CALLBACK proc(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                 UINT_PTR, DWORD_PTR ref) {
        auto* const icon = reinterpret_cast<TrayIcon*>(ref);

        if (message == WM_NCDESTROY)
            ::RemoveWindowSubclass(window, &TrayIconAccess::proc, kSubclassId);

        if (icon && message == taskbarCreated()) {
            addIcon(*icon);
            return 0;
        }

        if (icon) {
            switch (message) {
                case kIconMessage:
                    switch (LOWORD(lparam)) {
                        case WM_CONTEXTMENU:
                            showMenu(*icon, POINT{GET_X_LPARAM(wparam), GET_Y_LPARAM(wparam)});
                            return 0;

                        case NIN_SELECT:
                        case NIN_KEYSELECT:
                            if (icon->activated_) icon->activated_();
                            return 0;

                        default:
                            return 0;
                    }

                case kRunMessage: {
                    // Owned from the moment the post succeeded, so it is deleted
                    // here whether it runs or throws.
                    std::unique_ptr<std::function<void()>> const work(
                        reinterpret_cast<std::function<void()>*>(lparam));
                    if (*work) (*work)();
                    return 0;
                }
            }
        }

        return ::DefSubclassProc(window, message, wparam, lparam);
    }
};

TrayIcon::TrayIcon() noexcept = default;

TrayIcon::~TrayIcon() {
    hide();
}

bool TrayIcon::show(std::wstring_view tooltip) {
    if (window_) return true;

    tooltip_.assign(tooltip);

    // The application's own icon, unless one was already set with icon(): a
    // caller naming the icon before show() means that icon, not the default.
    if (!icon_) icon_ = reinterpret_cast<HICON__*>(applicationIcon());

    impl_ = std::make_unique<Impl>();
    HWND const window = window_handle(impl_->window);
    if (!window) {
        impl_.reset();
        return false;
    }
    window_ = reinterpret_cast<HWND__*>(window);

    // Fully transparent, so the pixel of window the OS will not shrink further is
    // never seen; the flyout is a windowed popup of its own and stays opaque.
    // WinUI has no window-transparency API, so this is Win32.
    ::SetWindowLongPtrW(window, GWL_EXSTYLE,
                        ::GetWindowLongPtrW(window, GWL_EXSTYLE) | WS_EX_LAYERED);
    ::SetLayeredWindowAttributes(window, 0, 0, LWA_ALPHA);

    // The shell posts to this HWND; the subclass catches those ahead of WinUI.
    ::SetWindowSubclass(window, &TrayIconAccess::proc, kSubclassId,
                        reinterpret_cast<DWORD_PTR>(this));

    // Primed once: activating loads the content and gives it a XamlRoot (which
    // showAt needs), hidden again at once so nothing is seen until a menu is
    // asked for. The window lives on, XamlRoot and all.
    impl_->window.activate();
    impl_->window.appWindow().hide();

    if (!TrayIconAccess::addIcon(*this)) {
        hide();
        return false;
    }

    return true;
}

void TrayIcon::hide() {
    if (!window_) return;

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof data;
    data.hWnd = reinterpret_cast<HWND>(window_);
    data.uID = kIconId;
    ::Shell_NotifyIconW(NIM_DELETE, &data);

    // Closing destroys the HWND (WM_NCDESTROY removes the subclass); dropping the
    // wrapper lets WinUI finish with it.
    if (impl_) impl_->window.close();
    impl_.reset();
    window_ = nullptr;
}

void TrayIcon::tooltip(std::wstring_view text) {
    tooltip_.assign(text);

    if (!window_) return;

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof data;
    data.hWnd = reinterpret_cast<HWND>(window_);
    data.uID = kIconId;
    data.uFlags = NIF_TIP | NIF_SHOWTIP;

    std::size_t const fits = std::min(tooltip_.size(), std::size(data.szTip) - 1);
    tooltip_.copy(data.szTip, fits);
    data.szTip[fits] = L'\0';

    ::Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::icon(HICON__* icon) {
    icon_ = icon;

    // Before show() there is no icon in the tray yet; show() will use this one
    // when it adds it. After, replace what is there in place.
    if (!window_) return;

    NOTIFYICONDATAW data{};
    data.cbSize = sizeof data;
    data.hWnd = reinterpret_cast<HWND>(window_);
    data.uID = kIconId;
    data.uFlags = NIF_ICON;
    data.hIcon = reinterpret_cast<HICON>(icon);
    ::Shell_NotifyIconW(NIM_MODIFY, &data);
}

void TrayIcon::menu(MenuBuilder build) {
    build_ = std::move(build);
}

void TrayIcon::onActivated(std::function<void()> handler) {
    activated_ = std::move(handler);
}

Rect TrayIcon::iconRect() const {
    if (!window_) return {};

    NOTIFYICONIDENTIFIER identifier{};
    identifier.cbSize = sizeof identifier;
    identifier.hWnd = reinterpret_cast<HWND>(window_);
    identifier.uID = kIconId;

    RECT rect{};
    if (::Shell_NotifyIconGetRect(&identifier, &rect) != S_OK) return {};

    return {{static_cast<float>(rect.left), static_cast<float>(rect.top)},
            {static_cast<float>(rect.right - rect.left),
             static_cast<float>(rect.bottom - rect.top)}};
}

bool TrayIcon::post(std::function<void()> work) const {
    if (!window_) return false;

    auto owned = std::make_unique<std::function<void()>>(std::move(work));
    if (!::PostMessageW(reinterpret_cast<HWND>(window_), kRunMessage, 0,
                        reinterpret_cast<LPARAM>(owned.get())))
        return false;

    // The message carries it now, and the handler deletes it.
    owned.release();
    return true;
}

}  // namespace wxl
