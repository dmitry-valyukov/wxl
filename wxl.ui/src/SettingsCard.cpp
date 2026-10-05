// Первым — заголовок с проекцией: после `import std` заголовки стандартной библиотеки, что тянет cppwinrt, уже не включить.
#include "impl/settings_card.h"

#include "SettingsCard.h"

#include "Object.impl.h"
#include "impl/conversions.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

namespace {

// The object behind the wrapper: the derived class itself, which the wrapper made in its constructor.
impl::SettingsCardCore* core_of(winrt::Microsoft::UI::Xaml::Controls::ContentControl const& control) {
    return winrt::get_self<impl::SettingsCardCore>(control.as<winrt::Microsoft::UI::Xaml::Controls::IContentControlOverrides>());
}

}  // namespace

SettingsCard::SettingsCard() : ContentControl(new Impl{}) {
    auto inspectable = winrt::make_self<impl::SettingsCardCore>().as<winrt::Windows::Foundation::IInspectable>();
    *put_abi() = static_cast<::IInspectable*>(winrt::detach_abi(inspectable));
}

void SettingsCard::header(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->header(*Object::Impl::get_typed<Object>(value));
}

void SettingsCard::header(hstring_param const& value) const {
    core_of(get<&Impl::contentControl_>())->header(impl::box_text(value));
}

Object SettingsCard::header() const {
    return Object::Impl::wrap<Object>(core_of(get<&Impl::contentControl_>())->header());
}

void SettingsCard::description(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->description(*Object::Impl::get_typed<Object>(value));
}

void SettingsCard::description(hstring_param const& value) const {
    core_of(get<&Impl::contentControl_>())->description(impl::box_text(value));
}

Object SettingsCard::description() const {
    return Object::Impl::wrap<Object>(core_of(get<&Impl::contentControl_>())->description());
}

void SettingsCard::headerIcon(IconElement const& value) const {
    core_of(get<&Impl::contentControl_>())->headerIcon(*Object::Impl::get_typed<IconElement>(value));
}

void SettingsCard::actionIcon(IconElement const& value) const {
    core_of(get<&Impl::contentControl_>())->actionIcon(*Object::Impl::get_typed<IconElement>(value));
}

bool SettingsCard::isClickEnabled() const {
    return core_of(get<&Impl::contentControl_>())->isClickEnabled();
}

void SettingsCard::isClickEnabled(bool value) const {
    core_of(get<&Impl::contentControl_>())->isClickEnabled(value);
}

bool SettingsCard::isActionIconVisible() const {
    return core_of(get<&Impl::contentControl_>())->isActionIconVisible();
}

void SettingsCard::isActionIconVisible(bool value) const {
    core_of(get<&Impl::contentControl_>())->isActionIconVisible(value);
}

SettingsCardContentAlignment SettingsCard::contentAlignment() const {
    return core_of(get<&Impl::contentControl_>())->contentAlignment();
}

void SettingsCard::contentAlignment(SettingsCardContentAlignment value) const {
    core_of(get<&Impl::contentControl_>())->contentAlignment(value);
}

EventToken SettingsCard::add_onClick(EventHandler<RoutedEventArgs> const& handler) const {
    return impl::from_winrt(core_of(get<&Impl::contentControl_>())->Click({
        [handler](auto const& sender, auto const& args) {
            auto view = Object::Impl::make_args<RoutedEventArgs>(winrt::get_abi(args));
            handler(Object::Impl::wrap<Object>(sender), view);
        }}));
}

void SettingsCard::remove_onClick(EventToken token) const {
    core_of(get<&Impl::contentControl_>())->Click(impl::to_winrt(token));
}

}  // namespace wxl
