// The winrt and Windows headers come first, and with them the standard
// library they pull in textually: launch.h reaches wxl.core through core.h,
// and a standard header included after that import is one MSVC has already
// seen through the std module. launch.h is last for the same reason -- and it
// now has to be, because Teardown holds a core::function, so parsing it
// materialises the std module in this unit; a textual winrt header after that
// would collide.
#include "platform.h"

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.XamlTypeInfo.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>

#include <crtdbg.h>
#include <iostream>

#include "impl/activation_factory.h"
#include "impl/app_icon.h"
#include "impl/bootstrap.h"
#include "impl/event_waits.h"
#include "launch.h"

import wxl.async;

// The private side of the entry point: the XAML application object wxl runs
// the show through, and the wWinMain that brings everything up around it.

namespace {

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

// What the process waits for before it ends: no detached frame alive and no operation out.
bool nothingLeftToWaitFor() noexcept {
    return wxl::async::detached_task::live_frames() == 0 &&
           wxl::async::sta_loop::outstanding() == 0;
}

#ifndef NDEBUG
// The watch over the wait at exit, in a Debug build (see wWinMain): called once, when the
// thread timer comes due, and the timer goes with the call.
void CALLBACK reportWhatIsStillAlive(HWND, UINT, UINT_PTR id, DWORD) {
    ::KillTimer(nullptr, id);

    const std::size_t frames = wxl::async::detached_task::live_frames();
    const std::size_t operations = wxl::async::sta_loop::outstanding();

    if (frames == 0 && operations == 0) return;

    _RPTF2(_CRT_ASSERT,
           "wxl: ten seconds into the wait at exit, %zu detached coroutine frames are still "
           "alive and %zu operations still out; the process goes on waiting for them",
           frames, operations);
}
#endif

}  // namespace

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

    // What a coroutine nobody holds does when it fails. wxl.async owns the
    // hook but cannot name the exception that matters here: almost everything
    // that goes wrong in a user interface comes back as a winrt::hresult_error,
    // which is not a std::exception and would otherwise be reported as
    // "a foreign exception" and nothing more. This unit knows the projection,
    // so it is the one that can say what happened.
    wxl::async::on_detached_task_failure() = [](std::exception_ptr error) noexcept {
        try {
            std::rethrow_exception(error);
        } catch (winrt::hresult_error const& failure) {
            std::wcerr << L"wxl: a detached coroutine failed: "
                       << static_cast<std::wstring_view>(failure.message()) << L" (0x" << std::hex
                       << static_cast<uint32_t>(failure.code()) << L")\n";
        } catch (std::exception const& failure) {
            std::cerr << "wxl: a detached coroutine failed: " << failure.what() << '\n';
        } catch (...) {
            std::cerr << "wxl: a detached coroutine failed with a foreign exception\n";
        }

        std::wcerr.flush();
        std::cerr.flush();
        std::terminate();
    };

    // The framework package first: without it no WinUI3 class can be
    // activated at all, and the failure is an unhelpful "class not
    // registered" far away from here.
    wxl::impl::ensure_windows_app_runtime_initialized();

    winrt::init_apartment(winrt::apartment_type::single_threaded);

    // The loop asynchronous files and directories run on, started here so that an
    // application has nothing to set up before it awaits one.
    wxl::async::sta_loop::start_dispatched("wxl: I/O");

    auto reason = wxl::TeardownReason::Closed;
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
    wxl::impl::set_application_launched(false);

    // Every coroutine still suspended on an event is told that what it waits
    // for will not come, so that it can unwind, run its destructors and let
    // go of what it holds. Before the teardown handler, because by the time
    // an application is asked what to save there should be nothing of ours
    // still running behind it.
    //
    // Synchronous cleanup is all this can offer. XAML has shut down by now
    // and the dispatcher with it, so a coroutine that answers cancellation by
    // awaiting something -- rolling a transaction back, closing a connection
    // politely -- has nothing left to resume it. An application that needs
    // that has to close its waits while its loop is still turning, from its
    // main window's Closed; wxl has no notion of a main window to do it for
    // them yet.
    wxl::impl::close_event_waits();

    // A detached coroutine is nobody's to end but its own, and a process has no
    // business ending while one is alive: the thread's loop turns on -- the
    // worker's answers, the thread pool's, the queue's timers -- until the last
    // such frame is gone. One that waits for what never comes keeps the process
    // here, the way a thread nobody joins would, rather than being torn down
    // underneath. Before the teardown handler, so that the application is asked
    // what to save when nothing of its own still runs.
    //
    // The operations are the process's as well: an orphan a frame let go of to finish
    // alone, and one on its way back, are waited for with the frames -- which is why
    // sta_loop::stop() below finds none out.
    //
    // In a Debug build the wait is watched: a stop for a debugger is a developer's tool, and
    // a user's process has nobody to stop for. Ten seconds is far longer than a frame or an
    // operation that is only finishing its work -- a last write, a last answer from the pool
    // -- takes, so one still there by then is most likely waiting for what never comes, and
    // the watch reports it the way a failed check does: a place for a debugger to stop at,
    // with the frames and the operations counted in the text. It is not a timeout and not a
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
    // the handler, under the same condition -- frames included, since asking costs
    // nothing.
    wxl::async::sta_loop::run_until(&nothingLeftToWaitFor);

    wxl::async::sta_loop::stop();

    // The one place wxl runs what it registered through module_cleanup: after
    // the handler, which may still use what those cleanups release, and before
    // static destruction, whose order nothing here controls.
    wxl::core::module_cleanup::run_now();
    return exitCode;
}
