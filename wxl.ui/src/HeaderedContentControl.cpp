// Первым — заголовок с проекцией: после `import std` заголовки стандартной библиотеки, что тянет cppwinrt, уже не включить.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "HeaderedContentControl.h"
#include "Object.impl.h"
#include "impl/conversions.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>
#include "impl/element_helpers.h"

namespace wxl::impl {

// The control behind wxl::HeaderedContentControl: a ContentControl with a header over its content (Toolkit's
// HeaderedContentControl). Aggregation as in impl/SettingsCardCore.h.

struct HeaderedContentControlCore
    : winrt::Microsoft::UI::Xaml::Controls::ContentControlT<HeaderedContentControlCore> {
    HeaderedContentControlCore();

    void OnApplyTemplate();

    winrt::Windows::Foundation::IInspectable header() const { return header_; }
    void header(winrt::Windows::Foundation::IInspectable const& value);

private:
    winrt::Windows::Foundation::IInspectable header_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter headerPart_{nullptr};
};

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

namespace {

// The header over the content; no header, no room taken for it.
constexpr wchar_t const* templateText =
    L"<ControlTemplate TargetType='ContentControl' "
    L"xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
    L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
    L"<StackPanel Spacing='8'>"
    L"<ContentPresenter x:Name='PART_Header' FontWeight='SemiBold' Visibility='Collapsed'/>"
    L"<ContentPresenter Content='{TemplateBinding Content}' ContentTemplate='{TemplateBinding "
    L"ContentTemplate}' "
    L"HorizontalContentAlignment='{TemplateBinding HorizontalContentAlignment}' "
    L"VerticalContentAlignment='{TemplateBinding VerticalContentAlignment}'/>"
    L"</StackPanel></ControlTemplate>";

}  // namespace

HeaderedContentControlCore::HeaderedContentControlCore() {
    Template(xaml::Markup::XamlReader::Load(templateText).as<controls::ControlTemplate>());
}

void HeaderedContentControlCore::OnApplyTemplate() {
    xaml::IFrameworkElementOverridesT<HeaderedContentControlCore>::OnApplyTemplate();
    headerPart_ = GetTemplateChild(L"PART_Header").try_as<controls::ContentPresenter>();
    header(header_);
}

void HeaderedContentControlCore::header(winrt::Windows::Foundation::IInspectable const& value) {
    header_ = value;
    if (headerPart_) {
        bool const blank = is_blank(value);
        headerPart_.Content(blank ? winrt::Windows::Foundation::IInspectable{nullptr} : value);
        headerPart_.Visibility(blank ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
    }
}

}  // namespace wxl::impl

namespace wxl {

namespace {

impl::HeaderedContentControlCore* core_of(
    winrt::Microsoft::UI::Xaml::Controls::ContentControl const& control) {
    return winrt::get_self<impl::HeaderedContentControlCore>(
        control.as<winrt::Microsoft::UI::Xaml::Controls::IContentControlOverrides>());
}

}  // namespace

// The derived class itself, made here and handed to the Impl it stands for.
HeaderedContentControl::HeaderedContentControl()
    : ContentControl(new Impl{winrt::make_self<impl::HeaderedContentControlCore>()
                                  .as<winrt::Windows::Foundation::IInspectable>()}) {}

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
