#include "settings_expander.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.Foundation.Collections.h>

namespace wxl::impl {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

namespace {

// The head's place and the list's place, found by name. The list sits under the head, on the second card brush,
// framed on the three sides the head does not frame.
constexpr wchar_t const* templateText =
    L"<ControlTemplate TargetType='ContentControl' xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
    L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
    L"<StackPanel>"
    L"<ContentPresenter x:Name='PART_Head' HorizontalContentAlignment='Stretch'/>"
    L"<Border x:Name='PART_Items' Visibility='Collapsed' CornerRadius='0,0,4,4' BorderThickness='1,0,1,1' "
    L"BorderBrush='{ThemeResource CardStrokeColorDefaultBrush}' Background='{ThemeResource CardBackgroundFillColorSecondaryBrush}'/>"
    L"</StackPanel></ControlTemplate>";

controls::FontIcon chevron(bool open) {
    controls::FontIcon icon;
    icon.Glyph(open ? L"" : L"");
    icon.FontSize(12);
    return icon;
}

}  // namespace

SettingsExpanderCore::SettingsExpanderCore() : head_(winrt::make_self<SettingsCardCore>()) {
    Template(xaml::Markup::XamlReader::Load(templateText).as<controls::ControlTemplate>());
    HorizontalContentAlignment(xaml::HorizontalAlignment::Stretch);

    head_->isClickEnabled(true);
    head_->actionIcon(chevron(false));
    head_->Click([this](auto const&, auto const&) { isExpanded(!expanded_); });

    // A card put in the list after it is on screen is dressed when the list is next laid out.
    items_.LayoutUpdated([this](auto const&, auto const&) { dress_items(); });
    IsEnabledChanged([this](auto const&, auto const&) { head_->IsEnabled(IsEnabled()); });
}

void SettingsExpanderCore::OnApplyTemplate() {
    xaml::IFrameworkElementOverridesT<SettingsExpanderCore>::OnApplyTemplate();

    headHost_ = GetTemplateChild(L"PART_Head").try_as<controls::ContentPresenter>();
    itemsHost_ = GetTemplateChild(L"PART_Items").try_as<controls::Border>();
    if (headHost_) {
        headHost_.Content(head_.as<winrt::Windows::Foundation::IInspectable>());
    }
    if (itemsHost_) {
        itemsHost_.Child(items_);
    }
    update();
}

void SettingsExpanderCore::isExpanded(bool value) {
    if (expanded_ == value) {
        return;
    }
    expanded_ = value;
    update();
    auto const self = this->try_as<winrt::Windows::Foundation::IInspectable>();
    (value ? expanded_event_ : collapsed_event_)(self, self);
}

void SettingsExpanderCore::update() {
    head_->actionIcon(chevron(expanded_));
    head_->surface(expanded_ ? SettingsCardCore::Surface::OpenHeader : SettingsCardCore::Surface::Standalone);
    if (itemsHost_) {
        itemsHost_.Visibility(expanded_ ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
    }
    dress_items();
}

// A card of the list is flat; anything else in it is left as it is. Told apart by the name of its runtime class, which
// only a SettingsCard answers to.
void SettingsExpanderCore::dress_items() {
    for (auto const& child : items_.Children()) {
        if (winrt::get_class_name(child) == SettingsCardCore::className) {
            winrt::get_self<SettingsCardCore>(child.as<controls::IContentControlOverrides>())->surface(SettingsCardCore::Surface::Flat);
        }
    }
}

}  // namespace wxl::impl
