#pragma once

#include "core.h"
#include "geometry.h"

// wxl::TrayIcon -- an icon in the notification area, and the menu behind it.
//
// The shell needs a window to talk to: Shell_NotifyIcon is given an HWND and
// sends every click, hover and menu request there as a message. So a tray icon
// owns a window whether the application has one or not, and this one keeps its
// own -- a top-level window (message-only ones receive no broadcasts, and
// TaskbarCreated, which Explorer broadcasts when it restarts, is the one message
// an icon cannot afford to miss), kept hidden except for the flick it shows to
// raise its menu.
//
// That window is a WinUI one, because the menu is a WinUI MenuFlyout and a
// flyout needs a XamlRoot only a XAML window can give -- so the same window
// receives the shell's messages (through a subclass) and lends the flyout its
// root. Held behind a pointer (TrayIcon::Impl in the .cpp) so this header stays
// clear of the WinUI wrappers.
//
// The menu is built when it is opened rather than kept, so a checkmark
// standing for something the application owns is always current and nobody has
// to remember to refresh it.
//
// Everything here belongs to the thread that created the icon. post() is the
// single exception, and it exists because the interesting things a tray
// application waits for -- a process ending, most of all -- report on a thread
// of the system's choosing.

// Declared rather than included: what is kept is two handles, and an
// application should not have to parse a Windows header to own an icon.
struct HWND__;
struct HICON__;

namespace wxl {

class TrayIcon {
public:
    /// One line of the menu. Empty text is a separator; an item with children
    /// is a submenu, and then nothing is called when it is opened.
    struct Item {
        std::wstring text;
        std::function<void()> onClick;
        std::vector<Item> children;
        bool checked = false;
        bool enabled = true;
    };

    /// The menu as it should look at the moment it is opened.
    using MenuBuilder = std::function<std::vector<Item>()>;

    // Both out-of-line, defined where TrayMenuHost is a complete type: the
    // unique_ptr member below cannot be constructed or destroyed against a
    // forward declaration.
    TrayIcon() noexcept;
    ~TrayIcon();

    TrayIcon(TrayIcon const&) = delete;
    TrayIcon& operator=(TrayIcon const&) = delete;

    /// Creates the window and puts the icon in the notification area.
    ///
    /// The icon is the application's own -- the first one in its resources --
    /// and the system's generic one where there is none, because an icon that
    /// fails to load leaves an invisible but clickable gap in the tray.
    ///
    /// \return false if the window could not be created or the shell refused
    ///         the icon.
    bool show(std::wstring_view tooltip);

    /// The icon shown in the notification area, replacing whatever is there --
    /// the application's own by default. Set before show() it is what show()
    /// puts up; set after, it replaces the icon at once. The handle stays the
    /// caller's: the icon is only referenced here, never destroyed, so a handle
    /// that has to be freed (one from ExtractIconEx, say) is the caller's to
    /// free, once nothing shows it any more.
    ///
    /// HICON is `HICON__*`, so a caller passes one without this header having to
    /// reach for a Windows one.
    void icon(HICON__* icon);

    /// Takes the icon out of the notification area. The destructor does it too.
    void hide();

    /// The text the shell shows on hover. Up to 127 characters, which is the
    /// shell's limit rather than ours; longer text is cut.
    void tooltip(std::wstring_view text);

    /// What builds the menu for a right-click. Without one, a right-click does
    /// nothing.
    void menu(MenuBuilder build);

    /// A left click on the icon. The conventional meaning is "show me the
    /// thing this application is about".
    void onActivated(std::function<void()> handler);

    /// Runs `work` on the thread that owns the icon, and the only member that
    /// may be called from another one.
    ///
    /// \return false if there is no icon, and therefore no window to run it.
    bool post(std::function<void()> work) const;

    /// Where the shell is showing the icon, in screen pixels, or an empty rect
    /// (zero size) when it is not on screen -- folded into the hidden-icons
    /// overflow, or not added yet. What a "minimize to the tray" animation
    /// aims at.
    Rect iconRect() const;

private:
    HWND__* window_ = nullptr;
    HICON__* icon_ = nullptr;
    std::wstring tooltip_;
    MenuBuilder build_;
    std::function<void()> activated_;

    // The window is a WinUI one -- it hosts the menu flyout, which needs a
    // XamlRoot -- and its wrappers live here, behind a pointer so this header
    // stays clear of them. Defined in TrayIcon.cpp.
    class Impl;
    std::unique_ptr<Impl> impl_;

    friend struct TrayIconAccess;
};

}  // namespace wxl
