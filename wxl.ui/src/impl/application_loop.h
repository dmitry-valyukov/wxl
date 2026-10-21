#pragma once

// The application's message loop and the windows of wxl's own that live beside it: what
// launch.cpp and CompositionWindow.cpp tell each other on the way out.
//
// XAML counts only its own windows. With DispatcherShutdownMode::OnLastWindowClose,
// which Application::Start sets, the loop ends when the last XAML Window closes, and a
// CompositionWindow still open is no window of XAML's to wait for. XAML does not outlive
// its loop: still inside Start it shuts its own Microsoft.UI queue down and takes itself
// apart in that queue's FrameworkShutdownStarting (FrameworkApplication_Partial.cpp in
// the sources of WinUI 3). The queue's ShutdownStarting comes before, with XAML alive --
// the last moment a window can be closed the way it is meant to be; after it, what the
// window holds of XAML is gone underneath it. The queue of wxl.async, of Windows.System,
// is another one, and its shutdown in sta_loop::stop() has nothing to do with XAML.

namespace wxl::impl {

/// Whether Application::Start is running its loop: set by launch.cpp before the call and
/// put down in ShutdownStarting of the queue of XAML, when the loop is over. A window that
/// closes while it is asks the application to exit; one closed after must not, because
/// Exit() then posts a WM_QUIT that no XAML loop is there to take, and the next wait of
/// the thread's loop stops on it. Defined in launch.cpp.
bool application_loop_running() noexcept;

void set_application_loop_running(bool running) noexcept;

/// Closes every CompositionWindow still open, while XAML lives -- launch.cpp calls it in
/// ShutdownStarting of the queue of XAML, the last moment for it: each is sent WM_CLOSE, so
/// that Closing and Closed run and the owners let go of what they hold, and one that
/// refuses to close is destroyed regardless -- on the way out a refusal has nothing left
/// to keep the window for. Windows a handler opens in the meantime are closed with the
/// rest. Defined in CompositionWindow.cpp, beside the list of live windows.
void close_windows_at_exit() noexcept;

/// Whether no CompositionWindow is alive. Defined in CompositionWindow.cpp.
bool windows_closed() noexcept;

}  // namespace wxl::impl
