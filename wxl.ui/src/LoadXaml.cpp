#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.Foundation.h>

#include "LoadXaml.h"
#include "Object.impl.h"
#include "impl/conversions.h"

namespace wxl {

Object loadXaml(hstring_param const& xaml) {
    auto const loaded = winrt::Microsoft::UI::Xaml::Markup::XamlReader::Load(impl::to_winrt(xaml));
    return Object::Impl::wrap<Object>(loaded);
}

}  // namespace wxl
