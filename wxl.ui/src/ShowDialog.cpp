// showDialog, the winrt side: the XamlRoot, then the dialog shown under the caller's token and
// the wait for its answer handed back (impl/show_dialog.h).
//
// The projection headers come first, and with them the standard library they
// pull in: the wxl headers below carry the wxl.core import, and a standard
// header after that import is one MSVC has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "Object.impl.h"
#include "ShowDialog.h"
#include "impl/show_dialog.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include <wxl/Microsoft.UI.Xaml.h>
#include <wxl/Microsoft.UI.Xaml.impl.h>

namespace wxl {

async::task<> showDialog(ContentDialog const& dialog, UIElement const& host, async::cancellation_token token) {
    winrt::Microsoft::UI::Xaml::Controls::ContentDialog const& native =
        *Object::Impl::get_typed<ContentDialog>(dialog);

    // Which island the dialog hangs over -- the one `host` is already in.
    if (host) {
        if (XamlRoot const root = host.xamlRoot())
            native.XamlRoot(*Object::Impl::get_typed<XamlRoot>(root));
    }

    return impl::show_dialog(dialog, std::move(token));
}

// A window is named by what it shows: a dialog raised from a window belongs
// over that same content, in that same island.
async::task<> showDialog(ContentDialog const& dialog, Window const& host, async::cancellation_token token) {
    return showDialog(dialog, host.content(), std::move(token));
}

}  // namespace wxl
