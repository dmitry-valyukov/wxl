#include "backdrop_controller.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Composition.SystemBackdrops.h>
#include <wxl/Microsoft.UI.Composition.SystemBackdrops.impl.h>

namespace wxl::impl {

void clear_system_backdrop(winrt::Microsoft::UI::Xaml::Window const& window, bool clear) {
    if (clear) {
        window.SystemBackdrop(nullptr);
    }
}

void set_mica_controller(winrt::Microsoft::UI::Xaml::Window const& window, MicaController const& controller) {
    winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController const& inner = *Object::Impl::get_typed<MicaController>(controller);
    inner.AddSystemBackdropTarget(window.as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
}

void set_desktop_acrylic_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                                    DesktopAcrylicController const& controller) {
    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController const& inner =
        *Object::Impl::get_typed<DesktopAcrylicController>(controller);
    inner.AddSystemBackdropTarget(window.as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
}

}  // namespace wxl::impl
