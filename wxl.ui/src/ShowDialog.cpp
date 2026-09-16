// showDialog, the winrt side: the XamlRoot, then the await nobody else has
// to write.
//
// The projection headers come first, and with them the standard library they
// pull in: the wxl headers below carry the wxl.core import, and a standard
// header after that import is one MSVC has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "Object.impl.h"
#include "ShowDialog.h"
#include "generated/Microsoft.UI.Xaml.Controls.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

namespace wxl {
namespace {

// The await, in a coroutine of its own. Fire-and-forget is what makes this
// call return while the dialog is still up, and the coroutine frame is also
// what keeps the operation alive until the reader closes it -- a caller who
// merely dropped the IAsyncOperation on the floor would be trusting the
// framework to hold it for them.
winrt::fire_and_forget await_dialog(winrt::Microsoft::UI::Xaml::Controls::ContentDialog native) {
    co_await native.ShowAsync();
}

}  // namespace

void showDialog(ContentDialog const& dialog, UIElement const& host) {
    winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& native =
        *Object::Impl::get_typed<ContentDialog>(dialog);

    // Which island the dialog hangs over -- the one `host` is already in.
    if (host) {
        if (XamlRoot const root = host.xamlRoot())
            native.XamlRoot(*Object::Impl::get_typed<XamlRoot>(root));
    }

    await_dialog(native);
}

// A window is named by what it shows: a dialog raised from a window belongs
// over that same content, in that same island.
void showDialog(ContentDialog const& dialog, Window const& host) {
    showDialog(dialog, host.content());
}

}  // namespace wxl
