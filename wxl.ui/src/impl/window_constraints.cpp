// The projection comes first, and with it every standard header it needs:
// wxl's own headers carry the wxl.core import, and a standard header
// included after that import is one the compiler has already seen through
// the std module.
#include <winrt/Microsoft.UI.Windowing.h>
#include <winrt/Windows.Foundation.h>

#include "window_constraints.h"

namespace wxl::impl {
namespace {

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

}  // namespace wxl::impl
