#include "headered_content_control.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>

namespace wxl::impl {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

namespace {

// The header over the content; no header, no room taken for it.
constexpr wchar_t const* templateText =
    L"<ControlTemplate TargetType='ContentControl' xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
    L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
    L"<StackPanel Spacing='8'>"
    L"<ContentPresenter x:Name='PART_Header' FontWeight='SemiBold' Visibility='Collapsed'/>"
    L"<ContentPresenter Content='{TemplateBinding Content}' ContentTemplate='{TemplateBinding ContentTemplate}' "
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
        headerPart_.Content(value);
        headerPart_.Visibility(value ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
    }
}

}  // namespace wxl::impl
