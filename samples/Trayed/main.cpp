// Trayed -- runs the program named on its command line and hosts its console
// inside a window of ours, managed from the tray.
//
// The whole sample rests on one decision: the child gets no pipe, it gets
// *our* console. A console program started with neither CREATE_NEW_CONSOLE nor
// DETACHED_PROCESS attaches to the console of whoever started it, so its
// output lands in the window we allocated with nothing in between -- no
// redirection, no reader thread, nothing to wait on. Scrolling, selecting and
// copying come with the console, and so does a working stdin for a program
// that asks questions.
//
// What that costs is the text itself. It goes to conhost and never through us,
// so there is nothing here to save, filter or colour, and the encoding is
// chosen at one remove: SetConsoleOutputCP tells the console how to read the
// bytes the child writes. The console keeps characters rather than bytes, so
// the choice applies to what comes after it and cannot repair what is already
// on screen -- which is why "Перезапустить" sits next to the encodings.
//
// The window, though, is ours. conhost's own window is reparented into a
// top-level window this sample creates (SetParent), and that changes
// everything the window does: its close button and its minimize button reach
// our own window procedure as WM_CLOSE and WM_SYSCOMMAND, before any system
// animation, and both send the window to the tray instead of ending it or
// dropping it on the taskbar -- which is the point of a tray application. The
// program is stopped only from the menu, by closing nothing, or when it ends
// on its own; then Trayed exits with the program's code.
//
// The child ending is the one thing worth waiting for, and that needs no
// thread of ours either: RegisterWaitForSingleObject reports it from the
// system's pool, and wxl::UiThread carries the news back to the thread that
// owns the window.

// NOMINMAX so the min/max macros do not wreck the std::max/std::min in the
// wxl headers this sample reaches through -- geometry.h, by way of TrayIcon.h.
#define NOMINMAX
#include <windows.h>

#include <commctrl.h>
#include <shellapi.h>

#include <functional>
#include <iterator>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "Settings.h"
#include "TrayIcon.h"
#include "UiThread.h"
#include "WindowHandle.h"
#include "WindowModal.h"
#include "WindowShade.h"
#include "launch.h"
#include "settings_window.h"

namespace {

/// The console code pages worth offering. Not a list of everything Windows
/// knows: what a Russian console program actually writes is one of these
/// three, and a menu of two hundred entries answers nobody's question.
struct Encoding {
    wchar_t const* name;
    UINT codePage;
};

constexpr Encoding kEncodings[] = {
    {L"UTF-8", 65001},
    {L"Кириллица (DOS, 866)", 866},
    {L"Кириллица (Windows, 1251)", 1251},
};

/// The "Settings" item added to the window's system menu. Below 0xF000, the
/// range the system's own SC_* commands take, and clear of their low nibble.
constexpr UINT kSettingsCommand = 0x1010;

/// Our own writing into the console. WriteConsoleW takes characters and hands
/// them to the console as they are, so what we say is never subject to the
/// code page the child's bytes are read with.
void write(std::wstring_view text) {
    DWORD written = 0;
    ::WriteConsoleW(::GetStdHandle(STD_OUTPUT_HANDLE), text.data(),
                    static_cast<DWORD>(text.size()), &written, nullptr);
}

void writeLine(std::wstring_view text) {
    write(text);
    write(L"\r\n");
}

/// Whether a window is one the user passes *through* to reach the tray rather
/// than an application they switched to: the taskbar, its flyouts, and the
/// icon's own hidden window. A click travelling to the tray is not a change of
/// the window the user was working in, and the foreground tracking below has
/// to leave these out or it would forget where "back" is.
bool transientWindow(HWND window) {
    wchar_t name[64]{};
    ::GetClassNameW(window, name, static_cast<int>(std::size(name)));

    for (wchar_t const* transient : {L"Shell_TrayWnd", L"Shell_SecondaryTrayWnd",
                                     L"TopLevelWindowForOverflowXamlIsland",
                                     L"NotifyIconOverflowWindow"}) {
        if (std::wstring_view{name} == transient) return true;
    }

    // The tray icon's own window is a WinUI window now (it hosts the menu
    // flyout), so its class is the same as the application's own and no longer
    // tells them apart. It carries the title "wxl.TrayIcon" for exactly this --
    // it flashes to the foreground while its flyout is up, and that is a trip to
    // the tray, not a window the user switched to.
    wchar_t title[64]{};
    ::GetWindowTextW(window, title, static_cast<int>(std::size(title)));
    return std::wstring_view{title} == L"wxl.TrayIcon";
}

/// The console this application owns for the rest of its life. Allocated and
/// hidden at once: it is about to become a child of our window, and a flash of
/// it as a window of its own is exactly what reparenting it quickly avoids.
void openConsole() {
    // A GUI process started from a terminal already has that terminal's
    // console attached, and AllocConsole then fails. The console has to be
    // ours: it is what our window hosts.
    ::FreeConsole();
    ::AllocConsole();
    ::SetConsoleTitleW(L"Trayed");
    ::ShowWindow(::GetConsoleWindow(), SW_HIDE);

    HANDLE const out = ::GetStdHandle(STD_OUTPUT_HANDLE);

    // Scrollback, which is the one thing a hosted console needs more of than
    // the default. The width is left as it is: a buffer narrower than the
    // window is refused, and wider makes the console scroll sideways.
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (::GetConsoleScreenBufferInfo(out, &info)) {
        ::SetConsoleScreenBufferSize(out, COORD{info.dwSize.X, 9000});
    }
}

/// Everything on our command line after our own name -- that is, the program
/// to run and its arguments, written the way the user wrote them.
std::wstring commandLineTail() {
    wchar_t const* rest = ::GetCommandLineW();

    if (*rest == L'"') {
        for (++rest; *rest && *rest != L'"'; ++rest) {}
        if (*rest) ++rest;
    } else {
        for (; *rest && *rest != L' ' && *rest != L'\t'; ++rest) {}
    }

    for (; *rest == L' ' || *rest == L'\t'; ++rest) {}

    return rest;
}

/// The first token of a command line -- the program, as the user wrote it,
/// unquoted. What an icon is pulled from.
std::wstring firstToken(std::wstring const& commandLine) {
    wchar_t const* p = commandLine.c_str();
    while (*p == L' ' || *p == L'\t') ++p;

    std::wstring token;
    if (*p == L'"') {
        for (++p; *p && *p != L'"'; ++p) token += *p;
    } else {
        for (; *p && *p != L' ' && *p != L'\t'; ++p) token += *p;
    }
    return token;
}

/// The big and small icons of the hosted program, or nulls where it has none.
/// The handles are the caller's to destroy.
// bigIcon/smallIcon, not big/small: `small` is a Windows macro (it expands to
// `char` -- see rpcndr.h), so a member of that name would not survive the
// preprocessor.
struct ProgramIcons {
    HICON bigIcon = nullptr;
    HICON smallIcon = nullptr;
};

ProgramIcons extractProgramIcons(std::wstring const& program) {
    if (program.empty()) return {};

    // A bare name is found on PATH with a .exe assumed, the way the shell finds
    // it; a name that already carries a path is taken as written.
    std::wstring path = program;
    if (program.find_first_of(L"\\/") == std::wstring::npos) {
        wchar_t resolved[MAX_PATH];
        if (::SearchPathW(nullptr, program.c_str(), L".exe", MAX_PATH, resolved, nullptr)) {
            path = resolved;
        }
    }

    ProgramIcons icons;
    ::ExtractIconExW(path.c_str(), 0, &icons.bigIcon, &icons.smallIcon, 1);
    return icons;
}

/// The program being hosted.
class Child {
public:
    ~Child() { stop(); }

    /// \param onExited called with the exit code on a thread of the system's
    ///        choosing, so whatever it touches has to be able to take that.
    ///        Called once, and never after stop().
    bool start(std::wstring const& commandLine, std::function<void(DWORD)> onExited) {
        if (process_) return false;

        onExited_ = std::move(onExited);

        // CreateProcessW is allowed to write into the command line it is
        // given, so it gets a copy of ours rather than the caller's string.
        std::wstring writable = commandLine;

        STARTUPINFOW startup{};
        startup.cb = sizeof startup;
        PROCESS_INFORMATION created{};

        // Suspended, and no inherited handles. The console is attached because
        // we do *not* ask for anything else -- no CREATE_NEW_CONSOLE, no
        // DETACHED_PROCESS -- and the child opens its own handles to it. The
        // suspension is for the job below: a child that has already run has
        // had time to start children of its own, and those would be outside
        // the job that is supposed to hold the whole tree.
        if (!::CreateProcessW(nullptr, writable.data(), nullptr, nullptr, FALSE,
                              CREATE_SUSPENDED, nullptr, nullptr, &startup, &created)) {
            return false;
        }

        process_ = created.hProcess;
        adopt();

        ::ResumeThread(created.hThread);
        ::CloseHandle(created.hThread);

        // The whole of the waiting: no thread of ours sleeps on this, and
        // nothing looks at it again and again.
        ::RegisterWaitForSingleObject(&wait_, process_, &Child::exited, this, INFINITE,
                                      WT_EXECUTEONLYONCE);
        return true;
    }

    /// Kills the child if it is still running, and lets go of it either way.
    ///
    /// Killing rather than leaving it: our console goes down with us, and a
    /// program writing into a console nobody owns any more gets errors it has
    /// no reason to expect.
    void stop() {
        if (!process_) return;

        ::TerminateProcess(process_, 0);
        release();
    }

    /// Lets go of a child that has ended on its own. The owning thread's call,
    /// prompted by onExited.
    void release() {
        if (wait_) {
            // INVALID_HANDLE_VALUE means "wait for the callback to finish",
            // which is safe here and nowhere near the callback itself.
            ::UnregisterWaitEx(wait_, INVALID_HANDLE_VALUE);
            wait_ = nullptr;
        }

        if (process_) {
            ::CloseHandle(process_);
            process_ = nullptr;
        }

        // Last, and not merely tidiness: this is the handle whose closing
        // kills whatever the child left running.
        if (job_) {
            ::CloseHandle(job_);
            job_ = nullptr;
        }
    }

    bool running() const noexcept { return process_ != nullptr; }

private:
    /// Puts the child, and anything it starts, in a job that dies with this
    /// process -- including when this process is killed and runs no cleanup at
    /// all. TerminateProcess in stop() covers the orderly way out; this covers
    /// every other one, and the grandchildren that TerminateProcess does not
    /// reach either way.
    void adopt() {
        job_ = ::CreateJobObjectW(nullptr, nullptr);
        if (!job_) return;

        JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};
        limits.BasicLimitInformation.LimitFlags = JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
        ::SetInformationJobObject(job_, JobObjectExtendedLimitInformation, &limits,
                                  sizeof limits);
        ::AssignProcessToJobObject(job_, process_);
    }

    static void CALLBACK exited(void* context, BOOLEAN) {
        auto* const child = static_cast<Child*>(context);

        DWORD code = 0;
        ::GetExitCodeProcess(child->process_, &code);

        if (child->onExited_) child->onExited_(code);
    }

    HANDLE process_ = nullptr;
    HANDLE wait_ = nullptr;
    HANDLE job_ = nullptr;
    std::function<void(DWORD)> onExited_;
};

/// The application: our window, the console reparented into it, a child in the
/// console, and an icon to manage all three from.
class Trayed {
public:
    explicit Trayed(std::wstring commandLine) : commandLine_(std::move(commandLine)) {
        stopped_ = ::CreateEventW(nullptr, TRUE, FALSE, nullptr);
        instance_ = this;
    }

    ~Trayed() {
        instance_ = nullptr;
        if (keyboardHook_) ::UnhookWindowsHookEx(keyboardHook_);
        if (mouseHook_) ::UnhookWindowsHookEx(mouseHook_);
        if (foregroundHook_) ::UnhookWinEvent(foregroundHook_);
        // The tray icon is gone by now (stop() hid it), so the handles it
        // borrowed are safe to free. Loaded shared icons need no freeing;
        // ExtractIconEx's do, and these are those.
        if (programIconBig_) ::DestroyIcon(programIconBig_);
        if (programIconSmall_) ::DestroyIcon(programIconSmall_);
        // host_ belongs to mainWindow_ (a wxl::Window), which destroys it.
        ::CloseHandle(stopped_);
    }

    void start() {
        loadSettings(settings_);

        // Saving is a side effect of the model changing, not of any one control:
        // whoever edits a setting -- a control through its two-way binding -- the
        // change is heard here and written back. No control has to remember to
        // call save. The three that also change the console apply as well.
        settings_.minimizeOnClose.on_change([this](bool const&) noexcept { saveSettings(settings_); });
        // The base size: the console goes to it, and it becomes what Ctrl+0
        // returns to. This is the one setting the transient zoom does not touch.
        settings_.fontSize.on_change([this](int const&) noexcept {
            zoomSize_ = settings_.fontSize.get();
            applyFont();
            saveSettings(settings_);
        });
        // The family: the face moves, the current size (a zoom in progress) stays.
        settings_.fontFamily.on_change([this](int const&) noexcept {
            applyFont();
            saveSettings(settings_);
        });
        settings_.theme.on_change([this](int const&) noexcept {
            applyConsoleTheme(settings_.theme.get());
            saveSettings(settings_);
        });
        // The title bar text: onto the window at once, and saved. The wchars are
        // the same UTF-16 units as the u16_text, reinterpreted for Windows.
        settings_.windowTitle.on_change([this](auto const&) noexcept {
            if (mainWindow_) mainWindow_->title(settings_.windowTitle.get());
            saveSettings(settings_);
        });

        openConsole();

        // The saved look, put on the console before anything is shown in it.
        // The current size starts at the base -- a fresh run starts there,
        // whatever the last run's transient zoom left off at, which is why the
        // zoom is not saved.
        zoomSize_ = settings_.fontSize.get();
        applyFont();
        applyConsoleTheme(settings_.theme.get());

        // Ctrl+wheel and Ctrl+plus/minus/0, taken in our own process because the
        // console that would otherwise handle them is another's window. The hooks
        // fire on this thread, dispatched by the message loop wxl runs; they act
        // only while our window is in front (zoomActive), so other applications
        // are untouched.
        keyboardHook_ = ::SetWindowsHookExW(WH_KEYBOARD_LL, &Trayed::keyboardHook,
                                            ::GetModuleHandleW(nullptr), 0);
        mouseHook_ = ::SetWindowsHookExW(WH_MOUSE_LL, &Trayed::mouseHook,
                                         ::GetModuleHandleW(nullptr), 0);

        // Taken on this thread, where a wrapper may be touched; what is kept is
        // the queue underneath, which the pool thread posts to.
        ui_ = wxl::UiThread{wxl::DispatcherQueue::getForCurrentThread()};
        ::SetConsoleCtrlHandler(&Trayed::onConsoleEvent, TRUE);

        createHost();
        adoptConsole();
        shade_.emplace(host_);
        applyProgramIcon();

        hasTray_ = tray_.show(tooltipText());
        if (hasTray_) {
            tray_.onActivated([this] { toggle(); });
            tray_.menu([this] { return buildMenu(); });
        } else {
            // Without an icon there is nowhere to send the window, so it stays
            // an ordinary window on the taskbar: minimize minimizes, the close
            // button closes.
            writeLine(L"[Trayed] значок в области уведомлений не появился; "
                      L"окно остаётся обычным на панели задач.");
        }

        // The foreground window, followed as it changes, so a tray click knows
        // whether our window was the one in front (hide it) or not (raise it).
        // The windows on the way to the tray are left out, not our own -- so no
        // WINEVENT_SKIPOWNPROCESS, unlike a hook watching a foreign window.
        foregroundHook_ = ::SetWinEventHook(EVENT_SYSTEM_FOREGROUND, EVENT_SYSTEM_FOREGROUND,
                                            nullptr, &Trayed::onForeground, 0, 0,
                                            WINEVENT_OUTOFCONTEXT);

        if (mainWindow_) mainWindow_->activate();
        noteForeground(::GetForegroundWindow());

        if (commandLine_.empty()) {
            writeLine(L"Trayed: укажите программу и её аргументы в командной строке.");
            writeLine(L"Например: Trayed.exe cmd /k dir");
            return;
        }

        run();
    }

    void stop() {
        child_.stop();
        tray_.hide();
        ::SetEvent(stopped_);
    }

    /// What the hosted program exited with, and therefore what this process
    /// exits with. Zero until the program has ended on its own: a program we
    /// stopped ourselves said nothing.
    int exitCode() const noexcept { return exitCode_; }

private:
    /// The application's main window -- a real wxl::Window, the first and
    /// always-present one, so WinUI does not end the application when a second
    /// window (the settings dialog) is closed. It carries no XAML content: its
    /// client area is the console, reparented in below. Its own window
    /// procedure is subclassed for the tray/minimize/close behaviour, since the
    /// window is WinUI's rather than a class of ours.
    void createHost() {
        mainWindow_.emplace();
        mainWindow_->title(settings_.windowTitle.get());
        host_ = reinterpret_cast<HWND>(wxl::window_handle(*mainWindow_));
        if (!host_) return;

        ::SetWindowSubclass(host_, &Trayed::hostSubclass, 0, reinterpret_cast<DWORD_PTR>(this));

        // "Настройки" in the window's system menu -- the menu that opens on a
        // right click of the title bar and on a click of the title-bar icon,
        // so one item answers both.
        if (HMENU const menu = ::GetSystemMenu(host_, FALSE)) {
            ::AppendMenuW(menu, MF_SEPARATOR, 0, nullptr);
            ::AppendMenuW(menu, MF_STRING, kSettingsCommand, L"Настройки…");
        }
    }

    /// Dresses the window and the tray icon in the hosted program's own icon,
    /// so Trayed wears the face of whatever it is running. A program with no
    /// icon of its own (a script, say) leaves the generic one in place. The
    /// handles are ours, freed in the destructor.
    ///
    /// Set on the tray before it is shown, so the icon comes up right rather
    /// than flashing the generic one first.
    void applyProgramIcon() {
        ProgramIcons const icons = extractProgramIcons(firstToken(commandLine_));
        if (!icons.bigIcon && !icons.smallIcon) return;

        programIconBig_ = icons.bigIcon;
        programIconSmall_ = icons.smallIcon;

        if (host_) {
            if (icons.smallIcon)
                ::SendMessageW(host_, WM_SETICON, ICON_SMALL,
                               reinterpret_cast<LPARAM>(icons.smallIcon));
            if (icons.bigIcon)
                ::SendMessageW(host_, WM_SETICON, ICON_BIG,
                               reinterpret_cast<LPARAM>(icons.bigIcon));
        }
        if (icons.smallIcon) tray_.icon(icons.smallIcon);
    }

    /// Takes conhost's window into ours as a child filling the client area.
    /// Stripping its frame first: it is no longer a window of its own, so its
    /// caption, borders and buttons would only be a frame drawn inside ours.
    void adoptConsole() {
        console_ = ::GetConsoleWindow();
        if (!console_ || !host_) return;

        LONG_PTR style = ::GetWindowLongPtrW(console_, GWL_STYLE);
        style &= ~(WS_POPUP | WS_OVERLAPPED | WS_CAPTION | WS_THICKFRAME | WS_MINIMIZEBOX |
                   WS_MAXIMIZEBOX | WS_SYSMENU);
        style |= WS_CHILD;
        ::SetWindowLongPtrW(console_, GWL_STYLE, style);
        ::SetParent(console_, host_);
        ::SetWindowPos(console_, nullptr, 0, 0, 0, 0,
                       SWP_NOMOVE | SWP_NOSIZE | SWP_NOZORDER | SWP_FRAMECHANGED);

        RECT client{};
        ::GetClientRect(host_, &client);
        ::MoveWindow(console_, 0, 0, client.right, client.bottom, TRUE);
        ::ShowWindow(console_, SW_SHOW);
        fitConsole();
    }

    /// Sizes conhost's own viewport and buffer to fill the client. conhost keeps
    /// a viewport (srWindow) of its own, and by default it is smaller than the
    /// window we reparented conhost into. A program grows it to suit itself, but
    /// with none there conhost paints only that corner and shows its plain white
    /// window background for the rest -- the white panel over the console a run
    /// with no program leaves. Sizing the viewport to the cells that fit fills
    /// the window with the console's own (themed) background instead.
    void fitConsole() {
        if (!console_) return;
        HANDLE const out = ::GetStdHandle(STD_OUTPUT_HANDLE);

        CONSOLE_FONT_INFOEX font{};
        font.cbSize = sizeof font;
        if (!::GetCurrentConsoleFontEx(out, FALSE, &font)) return;
        if (font.dwFontSize.X <= 0 || font.dwFontSize.Y <= 0) return;

        RECT client{};
        ::GetClientRect(host_, &client);
        LONG cols = client.right / font.dwFontSize.X;
        LONG rows = client.bottom / font.dwFontSize.Y;
        if (cols < 1) cols = 1;
        if (rows < 1) rows = 1;
        if (rows > 9000) rows = 9000;

        // The resize dance: shrink the viewport out of the way, size the buffer
        // (at least as wide as the viewport will be, the scrollback kept), then
        // set the viewport. Out of order, either call fails where the two cross.
        SMALL_RECT const minimal{0, 0, 0, 0};
        ::SetConsoleWindowInfo(out, TRUE, &minimal);
        ::SetConsoleScreenBufferSize(out, COORD{static_cast<SHORT>(cols), 9000});
        SMALL_RECT const viewport{0, 0, static_cast<SHORT>(cols - 1), static_cast<SHORT>(rows - 1)};
        ::SetConsoleWindowInfo(out, TRUE, &viewport);
    }

    static LRESULT CALLBACK hostSubclass(HWND window, UINT message, WPARAM wparam, LPARAM lparam,
                                         UINT_PTR, DWORD_PTR ref) {
        auto* const self = reinterpret_cast<Trayed*>(ref);
        if (!self) return ::DefSubclassProc(window, message, wparam, lparam);

        if (message == WM_NCDESTROY) ::RemoveWindowSubclass(window, &Trayed::hostSubclass, 0);

        switch (message) {
            case WM_SIZE:
                if (self->console_) {
                    ::MoveWindow(self->console_, 0, 0, LOWORD(lparam), HIWORD(lparam), TRUE);
                    self->fitConsole();
                }
                return 0;

            case WM_SETFOCUS:
                // The console is the child that reads the keyboard, so focus
                // has to reach it or the hosted program never sees a keystroke.
                if (self->console_) ::SetFocus(self->console_);
                return 0;

            case WM_SYSCOMMAND:
                // Minimize means "to the tray" while there is a tray to go to;
                // the window never actually minimizes, so nothing animates
                // toward the taskbar. Everything else -- move, size, restore --
                // is the system's to handle.
                if ((wparam & 0xFFF0) == SC_MINIMIZE && self->hasTray_) {
                    self->hide();
                    return 0;
                }
                if ((wparam & 0xFFF0) == kSettingsCommand) {
                    self->openSettings();
                    return 0;
                }
                break;

            case WM_CLOSE:
                // The close button sends the window to the tray, or ends the
                // program -- the setting decides, and without a tray there is
                // nowhere to fold into, so it always ends.
                if (self->hasTray_ && self->settings_.minimizeOnClose.get()) {
                    self->hide();
                } else {
                    self->quit();
                }
                return 0;
        }

        return ::DefSubclassProc(window, message, wparam, lparam);
    }

    /// Whether our window is on screen rather than hidden in the tray.
    bool shown() const {
        return host_ && ::IsWindowVisible(host_) && !::IsIconic(host_);
    }

    /// Whether our window is the one the user is working in right now. Shown is
    /// part of it: a window just hidden stays the foreground window for a
    /// moment, and that is not "in front".
    bool inFront() const { return shown() && lastForeground_ == host_; }

    /// Where on the screen the window flies to and from: the icon, or -- when
    /// it is folded into the hidden-icons overflow -- the corner of the work
    /// area the overflow lives by.
    wxl::Point trayCenter() const {
        wxl::Rect const rect = tray_.iconRect();
        if (rect.size.width > 0) {
            return {rect.offset.x + rect.size.width / 2, rect.offset.y + rect.size.height / 2};
        }

        RECT work{};
        ::SystemParametersInfoW(SPI_GETWORKAREA, 0, &work, 0);
        return {static_cast<float>(work.right), static_cast<float>(work.bottom)};
    }

    /// Brings the window back from the tray, flying up to its place and coming
    /// to the front the way a taskbar button would.
    void show() {
        if (!host_) return;

        // Already on screen: just bring it to the front, no flight. The fly-in
        // is for coming back from the tray -- a window that is merely behind
        // another is raised the way a taskbar button raises it.
        if (shown()) {
            ::SetForegroundWindow(host_);
            return;
        }

        if (shade_) {
            shade_->expandFrom(trayCenter(), [this] { ::SetForegroundWindow(host_); });
        } else {
            ::ShowWindow(host_, SW_SHOW);
            ::SetForegroundWindow(host_);
        }
    }

    /// Sends the window to the tray, flying down to the icon.
    void hide() {
        if (!host_) return;
        if (shade_) {
            shade_->collapseTo(trayCenter());
        } else {
            ::ShowWindow(host_, SW_HIDE);
        }
    }

    /// The taskbar's own rule for a click on its button: the window in front
    /// goes away, a window in any other state comes to the front.
    void toggle() {
        if (inFront()) {
            hide();
        } else {
            show();
        }
    }

    void noteForeground(HWND window) {
        if (window && !transientWindow(window)) lastForeground_ = window;
    }

    static void CALLBACK onForeground(HWINEVENTHOOK, DWORD, HWND window, LONG, LONG, DWORD, DWORD) {
        if (Trayed* const app = instance_) app->noteForeground(window);
    }

    /// Opens the settings window, or brings it back if it is already there.
    /// This is the one place Trayed uses wxl rather than raw Win32.
    void openSettings() {
        if (settingsWindow_) {
            // Already open -- raise it rather than build a second one.
            if (HWND const hwnd = reinterpret_cast<HWND>(wxl::window_handle(*settingsWindow_))) {
                ::SetForegroundWindow(hwnd);
            }
            return;
        }

        // A real second window, and it really closes. The main console window is
        // the first, always-present one, so WinUI does not end the application
        // when this one goes -- which is the whole reason the console host was
        // made a wxl::Window. Its Closed handler tears the two-way binding down:
        // unsubscribe_all cuts the model<->control cycle so the window and its
        // controls are freed, and then our handle is let go so the next open
        // builds it afresh.
        settingsWindow_.emplace(trayed::buildSettingsWindow(settings_));

        // A modal dialog of the main window: it disables the main window while
        // open and keeps itself off the taskbar. Still a real second window --
        // its own HWND and XamlRoot -- so the multi-window scenario stands.
        if (mainWindow_) wxl::makeModalDialog(*settingsWindow_, *mainWindow_);

        settingsWindow_->add_onClosed([this](wxl::Object const&, auto&) {
            wxl::core::unsubscribe_all(settings_.minimizeOnClose, settings_.fontFamily,
                                       settings_.fontSize, settings_.theme,
                                       settings_.windowTitle);
            // Not from inside its own Closed handler: let that return first, then
            // drop the wrapper -- resetting an object mid-event is asking for it.
            ui_.post([this] { settingsWindow_.reset(); });
        });
        settingsWindow_->activate();
    }

    /// Puts the current font on the console: the chosen family at the current
    /// size -- which is the base until the zoom moves it. A different cell size
    /// means a different number of cells fit, so the viewport is re-fitted.
    void applyFont() {
        applyConsoleFont(settings_.fontFamily.get(), zoomSize_);
        fitConsole();
    }

    /// Whether a zoom keystroke or wheel turn should be taken now: our window is
    /// the one in front, and Ctrl is down. Read from the low-level hooks, where
    /// the modifier is not in the event and the target window is not the hook's.
    bool zoomActive() const {
        return host_ && ::GetForegroundWindow() == host_ &&
               (::GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0;
    }

    /// Moves the console's font size by `delta` pixels, kept within readable
    /// bounds. Transient: the console changes, the settings do not, so the next
    /// run starts at the base again.
    void zoomBy(int delta) {
        int const next = zoomSize_ + delta;
        zoomSize_ = next < 6 ? 6 : (next > 72 ? 72 : next);
        applyFont();
    }

    /// Back to the base size -- what Ctrl+0 does. The base is the settings' size,
    /// the one the zoom never wrote to.
    void zoomReset() {
        zoomSize_ = settings_.fontSize.get();
        applyFont();
    }

    /// Ctrl+wheel and Ctrl+plus/minus/0, caught before the console's own zoom so
    /// there is no double move. Low-level because the console is a window of
    /// another process (conhost): a subclass cannot reach it, but a hook in our
    /// own process sees the input while our window is in front.
    static LRESULT CALLBACK keyboardHook(int code, WPARAM wparam, LPARAM lparam) {
        if (code == HC_ACTION && instance_ &&
            (wparam == WM_KEYDOWN || wparam == WM_SYSKEYDOWN) && instance_->zoomActive()) {
            auto const* key = reinterpret_cast<KBDLLHOOKSTRUCT*>(lparam);
            switch (key->vkCode) {
                case VK_OEM_PLUS:
                case VK_ADD:
                    instance_->zoomBy(+2);
                    return 1;
                case VK_OEM_MINUS:
                case VK_SUBTRACT:
                    instance_->zoomBy(-2);
                    return 1;
                case '0':
                case VK_NUMPAD0:
                    instance_->zoomReset();
                    return 1;
            }
        }
        return ::CallNextHookEx(nullptr, code, wparam, lparam);
    }

    static LRESULT CALLBACK mouseHook(int code, WPARAM wparam, LPARAM lparam) {
        if (code == HC_ACTION && instance_ && wparam == WM_MOUSEWHEEL &&
            instance_->zoomActive()) {
            auto const* mouse = reinterpret_cast<MSLLHOOKSTRUCT*>(lparam);
            short const wheel = static_cast<short>(HIWORD(mouse->mouseData));
            instance_->zoomBy(wheel > 0 ? +2 : -2);
            return 1;  // swallowed, so conhost does not also zoom
        }
        return ::CallNextHookEx(nullptr, code, wparam, lparam);
    }

    void run() {
        writeLine(L"> " + commandLine_);

        // Numbered, because the news of a child ending travels through the
        // queue and may arrive after restart() has already started the next
        // one; a report carrying an old number is about a child that is gone.
        unsigned const run = ++runs_;

        if (!child_.start(commandLine_, [this, run](DWORD code) {
                // The system's pool thread, where the only safe move is to ask
                // the window's own thread to do the rest.
                ui_.post([this, run, code] { childExited(run, code); });
            })) {
            writeLine(L"Trayed: не удалось запустить программу.");
        }

        tray_.tooltip(tooltipText());
    }

    void childExited(unsigned run, DWORD code) {
        if (run != runs_) return;

        // A program that has ended leaves nothing to host; what it said on the
        // way out is what we say.
        exitCode_ = static_cast<int>(code);
        child_.release();
        quit();
    }

    void restart() {
        child_.stop();
        run();
    }

    void quit() {
        stop();

        // What ends the message loop wxl is running for the application.
        ::PostQuitMessage(0);
    }

    void chooseEncoding(UINT codePage) {
        ::SetConsoleOutputCP(codePage);
        ::SetConsoleCP(codePage);

        // Said plainly, because the limit is real and surprising: the console
        // stores characters, so what is already on screen was decoded with the
        // old page and stays as it is.
        writeLine(L"[Trayed] кодировка " + std::to_wstring(codePage) +
                  L" -- для того, что будет выведено дальше.");
    }

    std::wstring tooltipText() const {
        if (commandLine_.empty()) return L"Trayed";

        return L"Trayed: " + commandLine_;
    }

    std::vector<wxl::TrayIcon::Item> buildMenu() {
        std::vector<wxl::TrayIcon::Item> encodings;
        UINT const current = ::GetConsoleOutputCP();

        for (Encoding const& encoding : kEncodings) {
            encodings.push_back({encoding.name,
                                 [this, page = encoding.codePage] { chooseEncoding(page); },
                                 {},
                                 encoding.codePage == current});
        }

        bool const hasProgram = !commandLine_.empty();

        return {
            {L"Показать консоль",
             [this] {
                 if (shown()) {
                     hide();
                 } else {
                     show();
                 }
             },
             {},
             shown()},
            {},
            {L"Кодировка вывода", {}, std::move(encodings)},
            {L"Перезапустить", [this] { restart(); }, {}, false, hasProgram},
            {},
            {L"Настройки…", [this] { openSettings(); }},
            {L"Выход", [this] { quit(); }},
        };
    }

    /// The console's control events, on a thread the system starts for them.
    ///
    /// Ctrl+C and Ctrl+Break belong to the hosted program: it is attached to
    /// the same console and is told the same thing, and a host that let them
    /// end it would take the program down through the job. A console close
    /// (end task, and other ways the session is torn down) ends this process
    /// whatever is answered here, so the answer is an orderly stop on the
    /// window's thread, waited for.
    static BOOL CALLBACK onConsoleEvent(DWORD event) {
        if (event == CTRL_C_EVENT || event == CTRL_BREAK_EVENT) return TRUE;

        if (Trayed* const app = instance_) {
            app->ui_.post([app] { app->quit(); });
            ::WaitForSingleObject(app->stopped_, 3000);
        }
        return TRUE;
    }

    inline static Trayed* instance_ = nullptr;

    wxl::TrayIcon tray_;
    Child child_;
    std::wstring commandLine_;
    wxl::UiThread ui_;
    // TODO: WindowShade is a move-only pimpl, and the empty state it already has
    // (moved-from) is not reachable from outside -- so it has no selector yet.
    std::optional<wxl::WindowShade> shade_;
    Settings settings_;
    wxl::core::nullable<wxl::Window> mainWindow_;
    wxl::core::nullable<wxl::Window> settingsWindow_;
    HWND host_ = nullptr;
    HWND console_ = nullptr;
    // The hosted program's icon, worn by the window and the tray. Ours to free.
    HICON programIconBig_ = nullptr;
    HICON programIconSmall_ = nullptr;
    bool hasTray_ = false;
    unsigned runs_ = 0;
    int exitCode_ = 0;
    // The console's current font height in pixels: the base until the zoom moves
    // it, and back to the base on Ctrl+0. Never saved -- the zoom is transient.
    int zoomSize_ = 18;
    HANDLE stopped_ = nullptr;
    HWINEVENTHOOK foregroundHook_ = nullptr;
    HHOOK keyboardHook_ = nullptr;
    HHOOK mouseHook_ = nullptr;
    HWND lastForeground_ = nullptr;
};

}  // namespace

wxl::Teardown wxl_launched() {
    // Held by the teardown handler, which is what keeps it alive for the run --
    // the same trick the windowed samples use to keep their window. What the
    // handler returns is what the process exits with: the hosted program's own
    // code.
    auto const app = std::make_shared<Trayed>(commandLineTail());
    app->start();

    return [app](wxl::TeardownReason) {
        app->stop();
        return app->exitCode();
    };
}
