// ThemeBrush, the winrt side: an element's own resource dictionary.
//
// The projection headers come first, and with them the standard library they
// pull in: the wxl headers below carry the wxl.core import, and a standard
// header after that import is one MSVC has already seen through the std
// module.
#include <winrt/Microsoft.UI.Xaml.Media.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "Object.impl.h"
#include "ThemeBrush.h"
#include "generated/Microsoft.UI.Xaml.Media.impl.h"
#include "generated/Microsoft.UI.Xaml.impl.h"

namespace wxl {

void ThemeBrush::write(FrameworkElement const& element) const {
    winrt::Microsoft::UI::Xaml::FrameworkElement const& native =
        *Object::Impl::get_typed<FrameworkElement>(element);

    // Insert rather than the indexer, and the answer is discarded: replacing
    // is what a second entry under one name means, and there is nothing to
    // report about it.
    native.Resources().Insert(winrt::box_value(winrt::hstring{key_}),
                              *Object::Impl::get_typed<Brush>(brush_));
}

}  // namespace wxl
