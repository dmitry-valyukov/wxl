// Первым — заголовок с проекцией: после `import std` заголовки стандартной библиотеки, что тянет cppwinrt, уже не включить.
#include "impl/settings_expander.h"

#include "SettingsExpander.h"

#include "Collection.impl.h"
#include "Object.impl.h"
#include "impl/conversions.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl {

namespace {

impl::SettingsExpanderCore* core_of(winrt::Microsoft::UI::Xaml::Controls::ContentControl const& control) {
    return winrt::get_self<impl::SettingsExpanderCore>(control.as<winrt::Microsoft::UI::Xaml::Controls::IContentControlOverrides>());
}

}  // namespace

SettingsExpander::SettingsExpander() : ContentControl(new Impl{}) {
    auto inspectable = winrt::make_self<impl::SettingsExpanderCore>().as<winrt::Windows::Foundation::IInspectable>();
    *put_abi() = static_cast<::IInspectable*>(winrt::detach_abi(inspectable));
}

// The setting is the content of the head, not of the expander: a ContentControl without a presenter in its template would take
// the element for a child of its own, and the head could no longer have it.
void SettingsExpander::content(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->Content(*Object::Impl::get_typed<Object>(value));
}

Object SettingsExpander::content() const {
    return Object::Impl::wrap<Object>(core_of(get<&Impl::contentControl_>())->head()->Content());
}

void SettingsExpander::header(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->header(*Object::Impl::get_typed<Object>(value));
}

void SettingsExpander::header(hstring_param const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->header(impl::box_text(value));
}

void SettingsExpander::description(Object const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->description(*Object::Impl::get_typed<Object>(value));
}

void SettingsExpander::description(hstring_param const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->description(impl::box_text(value));
}

void SettingsExpander::headerIcon(IconElement const& value) const {
    core_of(get<&Impl::contentControl_>())->head()->headerIcon(*Object::Impl::get_typed<IconElement>(value));
}

Collection<UIElement> SettingsExpander::items() const {
    return Object::Impl::wrap<Collection<UIElement>>(core_of(get<&Impl::contentControl_>())->items());
}

bool SettingsExpander::isExpanded() const {
    return core_of(get<&Impl::contentControl_>())->isExpanded();
}

void SettingsExpander::isExpanded(bool value) const {
    core_of(get<&Impl::contentControl_>())->isExpanded(value);
}

EventToken SettingsExpander::add_onExpanding(EventHandler<Object> const& handler) const {
    return impl::from_winrt(core_of(get<&Impl::contentControl_>())->Expanded({
        [handler](auto const& sender, auto const& args) {
            auto view = Object::Impl::wrap<Object>(args);
            handler(Object::Impl::wrap<Object>(sender), view);
        }}));
}

void SettingsExpander::remove_onExpanding(EventToken token) const {
    core_of(get<&Impl::contentControl_>())->Expanded(impl::to_winrt(token));
}

EventToken SettingsExpander::add_onCollapsed(EventHandler<Object> const& handler) const {
    return impl::from_winrt(core_of(get<&Impl::contentControl_>())->Collapsed({
        [handler](auto const& sender, auto const& args) {
            auto view = Object::Impl::wrap<Object>(args);
            handler(Object::Impl::wrap<Object>(sender), view);
        }}));
}

void SettingsExpander::remove_onCollapsed(EventToken token) const {
    core_of(get<&Impl::contentControl_>())->Collapsed(impl::to_winrt(token));
}

}  // namespace wxl
