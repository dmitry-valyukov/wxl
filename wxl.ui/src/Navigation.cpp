#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Interop.h>
#include <winrt/Microsoft.UI.Xaml.Media.Animation.h>
#include <winrt/Microsoft.UI.Xaml.h>

#include "Navigation.h"
#include "Object.impl.h"
#include "generated/Microsoft.UI.Xaml.Controls.impl.h"
#include "generated/Microsoft.UI.Xaml.Media.Animation.impl.h"

namespace wxl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

winrt::Windows::UI::Xaml::Interop::TypeName page_type() {
    return {winrt::hstring{winrt::name_of<xaml::Controls::Page>()}, winrt::Windows::UI::Xaml::Interop::TypeKind::Metadata};
}

Page page_of(xaml::Controls::Frame const& frame) {
    return Object::Impl::wrap<Page>(frame.Content().as<xaml::Controls::Page>());
}

}  // namespace

Page navigatePage(Frame const& frame) {
    auto const native = Object::Impl::as<xaml::Controls::Frame>(frame);
    native.Navigate(page_type(), nullptr);
    return page_of(native);
}

Page navigatePage(Frame const& frame, NavigationTransitionInfo const& info) {
    auto const native = Object::Impl::as<xaml::Controls::Frame>(frame);
    native.Navigate(page_type(), nullptr, Object::Impl::as<xaml::Media::Animation::NavigationTransitionInfo>(info));
    return page_of(native);
}

}  // namespace wxl
