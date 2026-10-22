// The winrt and Windows headers come first, and with them the standard
// library they pull in textually: launch.h reaches wxl.core through core.h,
// and a standard header included after that import is one MSVC has already
// seen through the std module. launch.h is last for the same reason -- and it
// now has to be, because Teardown holds a core::function, so parsing it
// materialises the std module in this unit; a textual winrt header after that
// would collide.
#include "platform.h"

#include <winrt/Microsoft.UI.Dispatching.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <crtdbg.h>
#include <iostream>

#include "impl/activation_factory.h"
#include "impl/app_icon.h"
#include "impl/application_loop.h"
#include "impl/bootstrap.h"
#include "impl/event_waits.h"
#include "impl/terminate_reason.h"
#include "coroutine_checks.h"
#include "launch.h"

import wxl.async;

// The private side of the entry point: the XAML application object wxl runs
// the show through, and the wWinMain that brings everything up around it.

namespace {

namespace dispatching = winrt::Microsoft::UI::Dispatching;

// What the application returned from wxl_launched. A namespace-scope object
// because the two ends of its life are in different functions: it is filled
// in when XAML reports the application launched, and run when the message
// loop that report came from has finished.
wxl::Teardown teardownHandler;

// XAML will not start without an Application object, and everything the
// framework looks up at run time -- the default control styles, the type
// information a control asks for while building its own template -- it looks
// up through that object. Authoring it with the projection's own composable
// base is deliberate: this is one object created once per process, on the
// private side of the library, and reimplementing WinRT aggregation for it
// would buy nothing.
struct App : winrt::Microsoft::UI::Xaml::ApplicationT<App,
                                                      winrt::Microsoft::UI::Xaml::Markup::
                                                          IXamlMetadataProvider> {
    void OnLaunched(winrt::Microsoft::UI::Xaml::LaunchActivatedEventArgs const&) {
        // From here on a WinRT object can be made; before, and after Start
        // returns, an attempt to make one is the mistake the check in
        // resolve_activation_factory names.
        wxl::impl::set_application_launched(true);

        // The windows of wxl's own are closed when the queue of XAML begins to shut down: its
        // ShutdownStarting is the last moment XAML is alive under them, and the loop is over by
        // then, so the flag goes down first (see impl/application_loop.h). Subscribed here, before
        // any subscription of the application's, and never taken off: the queue goes with it. The
        // order among the handlers of the event does not matter: closing the rendering devices
        // stops only their replacement. Neither call throws; a handler of Closing or Closed that
        // does ends in std::terminate, as from the window's procedure itself.
        auto const queue = dispatching::DispatcherQueue::GetForCurrentThread();
        if (!queue) wxl::core::abort("wxl: the XAML application launched with no dispatcher queue");
        static_cast<void>(queue.ShutdownStarting([](auto&&, auto&&) noexcept {
            wxl::impl::set_application_loop_running(false);
            wxl::impl::close_windows_at_exit();
        }));

        // Without a XAML application definition nothing has merged the
        // framework's own resource dictionary, so every control would come up
        // with no template and no style -- and no named style would resolve
        // either. Merging it here is what makes the controls look like
        // controls.
        Resources().MergedDictionaries().Append(
            winrt::Microsoft::UI::Xaml::Controls::XamlControlsResources{});

        // XAML records an error raised under it (a failed property set in a template, a layout call) and
        // reports it only on the next tick, by failing fast: the stack that reaches the debugger names no cause.
        // The event is raised before that; the message goes to stderr and the debugger, and the failure itself
        // is left to run its course -- nothing is marked handled.
        UnhandledException([](winrt::Windows::Foundation::IInspectable const&,
                              winrt::Microsoft::UI::Xaml::UnhandledExceptionEventArgs const& args) {
            std::wstring text = L"wxl: unhandled error 0x" + std::format(L"{:08X}", static_cast<std::uint32_t>(args.Exception())) +
                                L": " + std::wstring{args.Message()} + L"\n";
            std::wcerr << text << std::flush;
            ::OutputDebugStringW(text.c_str());
        });

        teardownHandler = wxl_launched();

        // The application has built and shown its windows by now, so this
        // is the moment a window that carries no icon can be given the one
        // in the executable -- see impl/app_icon.h.
        wxl::impl::apply_application_icon();
    }

    // A control building its own template asks the application to resolve the
    // types the template names. The provider that answers is the one the
    // framework ships with its controls; wxl adds nothing of its own to it.
    auto GetXamlType(winrt::Windows::UI::Xaml::Interop::TypeName const& type) const {
        return provider_.GetXamlType(type);
    }

    auto GetXamlType(winrt::hstring const& full_name) const {
        return provider_.GetXamlType(full_name);
    }

    auto GetXmlnsDefinitions() const { return provider_.GetXmlnsDefinitions(); }

    winrt::Microsoft::UI::Xaml::XamlTypeInfo::XamlControlsXamlMetaDataProvider provider_;
};

// What the process waits for before it ends: no operation out. A coroutine body is not waited
// for: every task lives in a place of an owner, and the owners let go before the end, so a frame
// still alive there is a broken rule, which a build that checks stops at (see wWinMain).
bool nothingLeftToWaitFor() noexcept {
    return wxl::async::sta_loop::outstanding() == 0;
}

#ifndef NDEBUG
// The watch over the wait at exit, in a Debug build (see wWinMain): called once, when the
// thread timer comes due, and the timer goes with the call. noexcept: an exception must not
// cross a Win32 callback -- unwinding through the frames of user32 is not supported, and on
// x64 a call from the kernel may swallow it -- so a failure inside is std::terminate, which
// names it.
void CALLBACK reportWhatIsStillAlive(HWND, UINT, UINT_PTR id, DWORD) noexcept {
    ::KillTimer(nullptr, id);

    const std::size_t operations = wxl::async::sta_loop::outstanding();

    if (operations == 0) return;

    // A Debug build checks, so it counts the frames too: what still waits for those operations.
    _RPTF2(_CRT_ASSERT,
           "wxl: ten seconds into the wait at exit, %zu operations are still out and %zu "
           "coroutine frames alive; the process goes on waiting for the operations",
           operations, wxl::async::live_frames());
}
#endif

}  // namespace

namespace wxl::impl {
namespace {

// Whether Application::Start is running its loop; see impl/application_loop.h.
bool s_application_loop_running = false;

}  // namespace

void set_application_loop_running(bool running) noexcept {
    s_application_loop_running = running;
}

bool application_loop_running() noexcept {
    return s_application_loop_running;
}

}  // namespace wxl::impl

// Version 6 of the common controls, asked for the only way it can be asked
// for: a manifest dependency on the side-by-side assembly. Without it a
// process gets the version 5 controls, and the plain Win32 windows an
// application still puts up -- MessageBox above all -- come up in the
// Windows 95 look, unthemed, beside a WinUI window that is anything but.
//
// Here rather than in every application's app.manifest, and here rather than
// anywhere else in wxl, for one reason: a `#pragma comment(linker, ...)` is
// written into this object's .drectve section, and the linker obeys the
// directives of the objects it actually links -- including one pulled out of
// a static library, but only if something referenced it. This translation
// unit defines wWinMain, so every wxl application links it by definition;
// any other unit here could be left out of some application and take the
// dependency with it, silently. The directive merges with the manifest an
// application supplies of its own, so app.manifest stays where it is and
// says what it said.
#pragma comment(linker,                                                            \
                "/manifestdependency:\"type='win32' "                              \
                "name='Microsoft.Windows.Common-Controls' version='6.0.0.0' "       \
                "processorArchitecture='*' publicKeyToken='6595b64144ccf1df' "      \
                "language='*'\"")

int __stdcall wWinMain(HINSTANCE, HINSTANCE, PWSTR, int) {
    // First of all: an exception that reaches a noexcept call -- a listener of a window or of
    // a field, the window's procedure -- ends in std::terminate, which otherwise leaves through
    // abort without a word. From here on it names the exception and stops through core::abort,
    // on this thread; a worker thread of wxl sets the same handler for itself, and the namer
    // set here is the one it asks about a winrt::hresult_error. A coroutine nobody holds that
    // fails ends here too: wxl.async's default hands its exception to this handler. An error
    // XAML records has a way of its own, UnhandledException above.
    wxl::impl::install_terminate_report();

#ifndef NDEBUG
    // A failed assert in a windowed application has nowhere to say so: the
    // CRT writes it to the debugger and, with none attached, shows the bare
    // "abort() has been called" box that names nothing. Sending the report to
    // stderr as well costs nothing and makes a run from a console -- or one
    // whose stderr is redirected, which is how the driver watches it -- say
    // which check failed. The proper answer, still to be written, is a handler of
    // wxl's own that puts the text on screen while the graphical phase is up.
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE | _CRTDBG_MODE_WNDW);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
#endif

    // The framework package first: without it no WinUI3 class can be
    // activated at all, and the failure is an unhelpful "class not
    // registered" far away from here.
    wxl::impl::ensure_windows_app_runtime_initialized();

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // The loop asynchronous files and directories run on, started here so that an
    // application has nothing to set up before it awaits one.
    wxl::async::sta_loop::start_dispatched("wxl: I/O");

    auto reason = wxl::TeardownReason::Closed;
    wxl::impl::set_application_loop_running(true);
    try {
        // Returns when the application exits; OnLaunched above runs inside.
        winrt::Microsoft::UI::Xaml::Application::Start(
            [](auto&&) { winrt::make<App>(); });
    } catch (winrt::hresult_error const& failure) {
        // The teardown handler is told it was an error, and nothing else:
        // without this line nobody learns what it was.
        std::wcerr << L"wxl: the application stopped on an error: "
                   << static_cast<std::wstring_view>(failure.message()) << L" (0x" << std::hex
                   << static_cast<uint32_t>(failure.code()) << L")\n";
        reason = wxl::TeardownReason::Error;
    } catch (std::exception const& failure) {
        std::cerr << "wxl: the application stopped on an error: " << failure.what() << '\n';
        reason = wxl::TeardownReason::Error;
    } catch (...) {
        std::cerr << "wxl: the application stopped on a foreign exception\n";
        reason = wxl::TeardownReason::Error;
    }
    // ShutdownStarting has put the flag down already; this is for a Start that ends without
    // it, by an error before its loop.
    wxl::impl::set_application_loop_running(false);
    wxl::impl::set_application_launched(false);

    // Every coroutine still suspended on an event is told that what it waits
    // for will not come, so that it can unwind, run its destructors and let
    // go of what it holds. Before the teardown handler, because by the time
    // an application is asked what to save there should be nothing of ours
    // still running behind it.
    //
    // What is over is XAML, not the thread's loop: XAML has taken itself apart
    // inside Start, while the thread's loop turns on in run_until below, and the
    // worker's answers, the thread pool's and the timers of wxl's queue still
    // come. Only the events of XAML do not. So a coroutine that answers
    // cancellation by awaiting a file or the pool (rolling a transaction back,
    // closing a connection politely) is resumed below; one that answers it by
    // waiting for an event of XAML's does not get it: the event will not come,
    // so a wait begun from here does not suspend and ends as cancelled at
    // once. An application that needs XAML on the way out has to close its
    // waits while its loop is still turning, from its main window's Closed;
    // wxl has no notion of a main window to do it for them yet.
    wxl::impl::close_event_waits();

    // A process has no business ending while an operation is out: the thread's loop
    // turns on -- the worker's answers, the thread pool's, the queue's timers -- until
    // the last one is back. Every task is kept by somebody who stops it, and the
    // application stops its own by cancelling its common source on the way out, from
    // its main window's Closed at the latest; a body cancelled ends by itself, its
    // operations with it, while its task -- a field of an object that lives on into the
    // teardown handler -- keeps only the end. A body that answers its cancellation by
    // awaiting a file goes on to that operation from inside the loop's turn that
    // resumed it, so the count does not fall to zero between its two. An operation
    // that never comes back keeps the process here, the way a thread nobody joins
    // would, rather than being torn down underneath. Before the teardown handler, so
    // that the application is asked what to save when nothing of its own still runs.
    //
    // A coroutine frame is not waited for: a frame alive at the end belongs to a task
    // whose owner outlived the end, which is a broken rule, and a build that checks stops
    // at it once nothing may hold a task any more, below.
    //
    // An orphan a frame let go of to finish alone, and an operation on its way back, are
    // counted as well -- which is why sta_loop::stop() below finds none out.
    //
    // In a Debug build the wait is watched: a stop for a debugger is a developer's tool, and
    // a user's process has nobody to stop for. Ten seconds is far longer than an operation
    // that is only finishing its work -- a last write, a last answer from the pool -- takes,
    // so one still there by then is most likely waiting for what never comes, and the
    // watch reports it the way a failed check does: a place for a debugger to stop at,
    // with the operations and the frames counted in the text. It is not a timeout and not a
    // way out: past the report the process goes on waiting, as it does in Release, where
    // nothing watches. The watch is a timer of the thread's own, with a procedure and no
    // window: the message loop turning here dispatches its WM_TIMER by calling the
    // procedure, so the timer itself wakes the wait when it comes due, and nothing looks at
    // a clock in between.
#ifndef NDEBUG
    const UINT_PTR exitWatch = ::SetTimer(nullptr, 0, 10'000, &reportWhatIsStillAlive);
    assert(exitWatch != 0 && "wxl: SetTimer refused the watch over the wait at exit");
#endif

    wxl::async::sta_loop::run_until(&nothingLeftToWaitFor);

#ifndef NDEBUG
    // Not wanted once the wait is over; one that has come due is gone already, and the call
    // finds nothing.
    ::KillTimer(nullptr, exitWatch);
#endif

    // The exit code is the handler's to name; an application with nothing to
    // say, or with no handler at all, exits with zero.
    int exitCode = 0;
    if (teardownHandler) {
        exitCode = (*teardownHandler)(reason).value_or(0);
        teardownHandler = {};
    }

    // After the handler, which may still let go of coroutines that are waiting for
    // a file: giving an operation up needs the worker. What such a coroutine had out
    // on the pool finishes as an orphan, and is waited for the same way as before
    // the handler, under the same condition.
    wxl::async::sta_loop::run_until(&nothingLeftToWaitFor);

    // Nothing may hold a task from here on: the application's owners have let go, the
    // teardown handler's last of all, and what they had out is back.
    if constexpr (wxl::async::detail::checked) {
        coro_check(wxl::async::live_frames() == 0,
                   "wxl: a coroutine frame is still alive at exit -- its owner outlived the exit",
                   wxl::async::detail::site::current());
    }

    // A CompositionWindow still alive here outlived XAML: one that was not closed when the
    // queue of XAML began to shut down -- a Start that failed before its loop, or a window
    // opened later, by the teardown handler, say. Destroyed in stop(), it would release its
    // XAML content under a XAML that is gone; the process stops on it here instead, with the
    // cause named.
    if (!wxl::impl::windows_closed()) {
        wxl::core::abort("wxl: a CompositionWindow is still alive at sta_loop::stop() -- it was "
                         "to be closed while XAML lived, in ShutdownStarting of its queue");
    }

    wxl::async::sta_loop::stop();

    // The one place wxl runs what it registered through module_cleanup: after
    // the handler, which may still use what those cleanups release, and before
    // static destruction, whose order nothing here controls.
    wxl::core::module_cleanup::run_now();
    return exitCode;
}
