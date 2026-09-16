// fitToContent, the winrt side: subscribe to the content's Loaded and resize the
// window's AppWindow to the content's arranged size.
//
// The projection and the Windows headers come first, and with them the standard
// library they pull in: the wxl headers below carry the wxl.core import, and a
// standard header after that import is one MSVC has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "Object.impl.h"
#include "events.h"
#include "generated/Microsoft.UI.Windowing.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Microsoft.UI.Xaml.impl.h"
#include "WindowFit.h"

namespace wxl {

namespace {

// Over the content: the title bar and border WinUI adds and does not let us
// measure. A rough allowance -- the window need not be to the pixel, only free
// of the empty acres a default size leaves.
constexpr double kChromeHeight = 40.0;

}  // namespace

void fitToContent(Window const& window) {
    AppWindow const appWindow = window.appWindow();

    if (FrameworkElement const content = window.content().try_as<FrameworkElement>()) {
        // The handler names the panel as its sender, so it reads the content from
        // the event rather than capturing it -- which would make the content own
        // a handler that owns the content. It holds only the AppWindow, which the
        // content does not own, so nothing here keeps the window alive past its
        // close. Loaded is a one-time thing, so this resizes once.
        content.add_onLoaded([appWindow](FrameworkElement const& panel, RoutedEventArgs&) {
            // desiredSize, not actualSize: content that stretches to fill the
            // window reports the whole client as its actual size, but desiredSize
            // is what it wanted -- the natural height to shrink the window to.
            Size const wanted = panel.desiredSize();
            double const scale = panel.xamlRoot().rasterizationScale();
            appWindow.resize(SizeInt32{
                static_cast<int32_t>(wanted.width * scale + 0.5),
                static_cast<int32_t>((wanted.height + kChromeHeight) * scale + 0.5)});
        });
    }
}

}  // namespace wxl
