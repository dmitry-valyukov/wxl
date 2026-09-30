#include "wheel_to_parent.h"

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

namespace wxl::impl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

void pass_wheel_up(winrt::Windows::Foundation::IInspectable const& sender,
                   xaml::Input::PointerRoutedEventArgs const& args) {
    auto const viewer = sender.as<xaml::Controls::ScrollViewer>();
    auto const properties = args.GetCurrentPoint(viewer).Properties();
    if (properties.IsHorizontalMouseWheel() ||
        args.KeyModifiers() != winrt::Windows::System::VirtualKeyModifiers::None ||
        viewer.ScrollableHeight() > 0) {
        return;
    }

    // The viewer has marked the wheel handled, and a handled event is not offered
    // to the elements above it. Taking the mark back sends it on: the panel
    // that scrolls the page gets the wheel as it would over any other element,
    // and scrolls it with its own inertia.
    args.Handled(false);
}

}  // namespace

void set_wheel_to_parent(xaml::Controls::ScrollViewer const& viewer, bool on) {
    if (!on) {
        return;
    }
    // handledEventsToo: the viewer marks the wheel handled itself, which is
    // the whole trouble.
    viewer.AddHandler(xaml::UIElement::PointerWheelChangedEvent(),
                      winrt::box_value(xaml::Input::PointerEventHandler(&pass_wheel_up)), true);
}

}  // namespace wxl::impl
