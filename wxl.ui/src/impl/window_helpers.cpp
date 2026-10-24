// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header
// included after that import is one the compiler has already seen through
// the std module.
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Windows.Foundation.h>

#include "window_helpers.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Composition.SystemBackdrops.h>
#include <wxl/Microsoft.UI.Composition.SystemBackdrops.impl.h>

namespace wxl::impl {
namespace {

// Over the content: the title bar and border WinUI adds and does not let us measure. A rough
// allowance -- the window need not be to the pixel, only free of the empty acres a default size
// leaves.
constexpr double chrome_height = 40.0;

// The window resized to what `content` wanted on the sides `mode` names. desiredSize, not
// actualSize: content that stretches to fill the window reports the whole client as its actual
// size, while desiredSize is what it wanted.
void fit_to(winrt::Microsoft::UI::Windowing::AppWindow const& appWindow,
            winrt::Microsoft::UI::Xaml::FrameworkElement const& content, SizeToContent mode) {
    auto const wanted = content.DesiredSize();
    double const scale = content.XamlRoot().RasterizationScale();
    auto size = appWindow.Size();
    if (mode == SizeToContent::Width || mode == SizeToContent::WidthAndHeight) {
        size.Width = static_cast<int32_t>(wanted.Width * scale + 0.5);
    }
    if (mode == SizeToContent::Height || mode == SizeToContent::WidthAndHeight) {
        size.Height = static_cast<int32_t>((wanted.Height + chrome_height) * scale + 0.5);
    }
    appWindow.Resize(size);
}

// An unset preference is what leaves that side of the window unbounded, so
// a zero is not a size of zero -- it is nothing said. The ABI box is built
// here rather than a nullable handed over: this is the runtime's own way of
// saying "or nothing", and the presenter takes nothing else.
winrt::Windows::Foundation::IReference<int32_t> bound(int32_t value) {
    return value > 0 ? winrt::Windows::Foundation::IReference<int32_t>{value} : nullptr;
}

}  // namespace

void set_minimum_size(winrt::Microsoft::UI::Xaml::Window const& window, SizeInt32 const& size) {
    auto const presenter =
        window.AppWindow().Presenter().try_as<winrt::Microsoft::UI::Windowing::OverlappedPresenter>();
    if (!presenter) {
        return;  // full-screen and compact-overlay windows have no bounds to set
    }
    presenter.PreferredMinimumWidth(bound(size.width));
    presenter.PreferredMinimumHeight(bound(size.height));
}

void set_size_to_content(winrt::Microsoft::UI::Xaml::Window const& window, SizeToContent mode) {
    namespace xaml = winrt::Microsoft::UI::Xaml;
    if (mode == SizeToContent::Manual) {
        return;
    }
    // The first activation only: the handler takes itself off through the token it shares with
    // this function. The window reaches the handler as the sender, so nothing here holds it.
    auto const activated = std::make_shared<winrt::event_token>();
    *activated = window.Activated([mode, activated](winrt::Windows::Foundation::IInspectable const& sender,
                                                    xaml::WindowActivatedEventArgs const&) {
        auto const shown = sender.as<xaml::Window>();
        shown.Activated(*activated);
        auto const content = shown.Content().try_as<xaml::FrameworkElement>();
        if (!content) {
            core::abort("wxl: sizeToContent on a window with no content to size to");
        }
        auto const appWindow = shown.AppWindow();
        if (content.IsLoaded()) {
            fit_to(appWindow, content, mode);
            return;
        }
        // Loaded comes once. The handler names the content as its sender rather than capturing
        // it, which would make the content own a handler that owns the content; it holds only
        // the AppWindow, which the content does not own.
        auto const loaded = std::make_shared<winrt::event_token>();
        *loaded = content.Loaded([mode, appWindow, loaded](winrt::Windows::Foundation::IInspectable const& panel,
                                                           xaml::RoutedEventArgs const&) {
            auto const element = panel.as<xaml::FrameworkElement>();
            element.Loaded(*loaded);
            fit_to(appWindow, element, mode);
        });
    });
}

void set_drag_region(winrt::Microsoft::UI::Xaml::UIElement const& element,
                     core::nullable<bool> const& value) {
    using winrt::Microsoft::UI::Xaml::Controls::TitleBar;
    if (value) {
        TitleBar::SetIsDragRegion(element, *value);
    } else {
        element.ClearValue(TitleBar::IsDragRegionProperty());
    }
}

void clear_system_backdrop(winrt::Microsoft::UI::Xaml::Window const& window, bool clear) {
    if (clear) {
        window.SystemBackdrop(nullptr);
    }
}

void set_mica_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                         MicaController const& controller) {
    winrt::Microsoft::UI::Composition::SystemBackdrops::MicaController const& inner =
        *Object::Impl::get_typed<MicaController>(controller);
    inner.AddSystemBackdropTarget(
        window.as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
}

void set_desktop_acrylic_controller(winrt::Microsoft::UI::Xaml::Window const& window,
                                    DesktopAcrylicController const& controller) {
    winrt::Microsoft::UI::Composition::SystemBackdrops::DesktopAcrylicController const& inner =
        *Object::Impl::get_typed<DesktopAcrylicController>(controller);
    inner.AddSystemBackdropTarget(
        window.as<winrt::Microsoft::UI::Composition::ICompositionSupportsSystemBackdrop>());
}

}  // namespace wxl::impl
