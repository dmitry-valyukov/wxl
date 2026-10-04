// Первым — заголовок с проекцией: после `import std` заголовки стандартной библиотеки, что тянет cppwinrt, уже не включить.
#include "impl/headered_content_control.h"

#include "HeaderedContentControl.h"

#include "Object.impl.h"
#include "impl/conversions.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

namespace {

impl::HeaderedContentControlCore* core_of(winrt::Microsoft::UI::Xaml::Controls::ContentControl const& control) {
    return winrt::get_self<impl::HeaderedContentControlCore>(control.as<winrt::Microsoft::UI::Xaml::Controls::IContentControlOverrides>());
}

}  // namespace

HeaderedContentControl::HeaderedContentControl() : ContentControl(new Impl{}) {
    auto inspectable = winrt::make_self<impl::HeaderedContentControlCore>().as<winrt::Windows::Foundation::IInspectable>();
    *put_abi() = static_cast<::IInspectable*>(winrt::detach_abi(inspectable));
}

void HeaderedContentControl::header(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->header(*Object::Impl::get_typed<Object>(value));
}

void HeaderedContentControl::header(hstring_param const& value) const {
    core_of(get<&Impl::contentControl_>())->header(impl::box_text(value));
}

Object HeaderedContentControl::header() const {
    return Object::Impl::wrap<Object>(core_of(get<&Impl::contentControl_>())->header());
}

}  // namespace wxl
