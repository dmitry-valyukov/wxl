#pragma once

// wxl::Application -- the application wxl runs: what an application asks about itself, and,
// out of its sight, what the start of the application does to it.
//
// Static members alone: there is one application, and wxl brings it up itself (launch.h). Not
// XAML's Application, which wxl reaches directly and which has no wrapper of its own.
//
// The private side is launch.cpp's -- the XAML application object and wWinMain -- and the one
// check that reads it, resolve_activation_factory. They are its friends, declared here without
// any Windows type, so including this costs a caller nothing.

#include "core.h"

struct HINSTANCE__;
struct _GUID;

// With the linkage winbase.h gives it, so that this declaration and that one agree whichever comes
// first in a translation unit.
extern "C" int __stdcall wWinMain(HINSTANCE__*, HINSTANCE__*, wchar_t*, int);

namespace wxl {

namespace impl {

struct App;
struct application_test;

long resolve_activation_factory(wchar_t const* class_name, _GUID const& iid,
                                void** factory) noexcept;

}  // namespace impl

class Application {
public:
    Application() = delete;

    /// The folder the running executable lives in: where a relative path such as
    /// "Assets/data.json" points, the same way an Image's source and a window's background
    /// picture resolve it, and where the build copies an application's own data files. It is
    /// core::environment::application_folder(): nothing about it is a window's, and a library
    /// below the window -- or a test of one -- asks for it too.
    static std::filesystem::path folder() { return core::environment::application_folder(); }

    /// Whether the process runs with the identity of an MSIX package. Part of the Windows App
    /// SDK works only for an application the shell knows by a package identity: the badge on a
    /// taskbar button (BadgeNotificationManager), the packaged form of app notifications, the
    /// registration of background tasks. An unpackaged application, which is what wxl
    /// applications are as they come out of CMake, has none of it, and a call to such an API
    /// fails; an application that offers the feature asks first.
    static bool isPackaged() noexcept;

    /// The path as it is when absolute, otherwise the same path in the application folder. A
    /// relative path an application writes -- "Assets/logo.png" -- points beside its executable,
    /// where the build copies its assets, and a picture named as a window's background and one
    /// named in an Image's source are found the same way.
    static std::filesystem::path makeAbsolute(std::filesystem::path const& path);

    /// Ends the application: XAML's Application.Exit(). wxl calls exit() itself when a window
    /// of its own closes. The loop of XAML ends; in ShutdownStarting of its queue, still
    /// inside Application::Start, the windows of wxl's own still open are closed; then Start
    /// returns, and the Teardown handler wxl_launched() returned is called -- its answer is the
    /// process's exit code, as on any other way out. The way to end an application that has no
    /// window, or whose window is only hidden: XAML ends one by itself only when its last window
    /// closes. On the UI thread only, the one that runs the loop of XAML: a call from another
    /// thread stops the process.
    ///
    /// Only while the loop runs (loopRunning()); after it the call does nothing, as wxl's own call
    /// does: the application is ending already, and asking XAML would be wrong for one of two
    /// reasons, by when the call comes. Between ShutdownStarting and the return of Start -- from a
    /// window closed on the way out -- XAML's application is still there, and Exit() would post a
    /// WM_QUIT that no XAML loop is there to take, on which the next wait of the thread's loop
    /// would stop. After Start has returned -- from the Teardown handler -- there is no XAML
    /// application any more: Application.Current() is null, and the call would dereference it. A
    /// second call while the loop runs is as harmless as XAML's.
    static void exit();

    /// Whether XAML's Application::Start is running its loop: set before the call and put down
    /// in ShutdownStarting of the queue of XAML, when the loop is over. A window that closes
    /// while it is asks the application to exit; one closed after must not, because Exit() then
    /// posts a WM_QUIT that no XAML loop is there to take, and the next wait of the thread's
    /// loop stops on it.
    static bool loopRunning() noexcept { return loop_running_; }

private:
    friend struct impl::App;
    friend struct impl::application_test;
    friend int __stdcall ::wWinMain(HINSTANCE__*, HINSTANCE__*, wchar_t*, int);
    friend long impl::resolve_activation_factory(wchar_t const* class_name, _GUID const& iid,
                                                 void** factory) noexcept;

    /// The application's icon, put on the windows that came up without one, right after the
    /// application has built them. A window does not take a program's icon from its resources,
    /// and WinUI does not look there either, so wxl does it once: resource id 1 -- the one the
    /// shell reads and the one TrayIcon looks up -- goes onto every top-level window of the
    /// thread that carries no icon yet. A window with an icon of its own is left alone, and so
    /// is an executable built without the resource. Windows of wxl's own class need none of
    /// this: CompositionWindow registers the icon with the class.
    static void apply_icon() noexcept;

    static void set_loop_running(bool running) noexcept { loop_running_ = running; }

    /// Whether the XAML application is up, which is the only time a WinRT object can be made:
    /// set for the application's life, and asserted on in resolve_activation_factory, which
    /// every first creation of a class passes through -- so a wxl object built at static
    /// initialization is caught with its cause named instead of failing as an unregistered
    /// class.
    static void set_launched(bool launched) noexcept { launched_ = launched; }

    /// Gives wxl.core's terminate handler the name of a winrt::hresult_error
    /// (name_hresult_error), and sets the handler on the calling thread: wWinMain's call, once in
    /// a process, since the namer is set once. A worker thread sets wxl.core's handler alone, and
    /// it asks the same namer.
    static void install_terminate_report() noexcept;

    /// What std::terminate says of a winrt::hresult_error: its code, and what the error says of
    /// itself or, failing that, what the system says of the code -- put into `report` from inside
    /// a catch of the exception being described; false, and nothing put, for an exception of
    /// another type. Nothing on the way throws, so describing it cannot bring std::terminate
    /// back. wxl.core's handler names a std::exception, an exception of a type it does not know
    /// and a terminate with nothing thrown; the projection's error is not a std::exception, and
    /// naming it is this layer's to add, through the namer wxl.core asks.
    static bool name_hresult_error(core::impl::terminate_report& report) noexcept;

    static inline bool loop_running_ = false;
    static inline bool launched_ = false;
};

}  // namespace wxl
