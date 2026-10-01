#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../core.h"

// What stands behind `micaController = ...`, `desktopAcrylicController = ...` and
// `noSystemBackdrop = true` on a Window.
//
// A system backdrop controller draws the material under a window, and is told
// which window by handing it the window's ICompositionSupportsSystemBackdrop --
// an interface that no wrapper declares and that the window answers to
// QueryInterface alone. The two tags take the controller, ask the window for that
// interface and add the window as the controller's target; the controller then
// needs its configuration (SystemBackdropConfiguration) and, when the window is
// closed, Close. The third takes the backdrop off the window.
//
// Private: the window arrives as the projection type the wrapper already holds;
// the controllers arrive as wrappers, and are unwrapped here.

namespace wxl {
class MicaController;
class DesktopAcrylicController;
}  // namespace wxl

namespace wxl::impl {

void clear_system_backdrop(winrt::Microsoft::UI::Xaml::Window const& window, bool clear);

void set_mica_controller(winrt::Microsoft::UI::Xaml::Window const& window, MicaController const& controller);

void set_desktop_acrylic_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                                    DesktopAcrylicController const& controller);

}  // namespace wxl::impl
