// Первым — заголовок с проекцией: после `import std` заголовки стандартной библиотеки, что тянет cppwinrt, уже не включить.
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "impl/SettingsCardCore.h"
#include "SettingsExpander.h"
#include "Collection.impl.h"
#include "Object.impl.h"
#include "impl/conversions.h"
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

namespace wxl::impl {

// The control behind wxl::SettingsExpander: a SettingsCard that opens onto a list of cards. The head is a SettingsCard of
// its own, made here and put into the template; a click on it opens and closes the list, and its action icon is the chevron
// that says which. The cards of the list are dressed flat -- a hairline between them, square, on the second card brush --
// when they are put in, and again whenever the list changes.

struct SettingsExpanderCore : winrt::Microsoft::UI::Xaml::Controls::ContentControlT<SettingsExpanderCore> {
    SettingsExpanderCore();

    void OnApplyTemplate();
    winrt::com_ptr<SettingsCardCore> const& head() const { return head_; }
    winrt::Microsoft::UI::Xaml::Controls::UIElementCollection items() const { return items_.Children(); }

    bool isExpanded() const { return expanded_; }
    void isExpanded(bool value);

    winrt::event_token Expanded(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler) {
        return expanded_event_.add(handler);
    }
    void Expanded(winrt::event_token const& token) noexcept { expanded_event_.remove(token); }
    winrt::event_token Collapsed(winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable> const& handler) {
        return collapsed_event_.add(handler);
    }
    void Collapsed(winrt::event_token const& token) noexcept { collapsed_event_.remove(token); }

private:
    void update();
    void dress_items();

    winrt::com_ptr<SettingsCardCore> head_;
    winrt::Microsoft::UI::Xaml::Controls::StackPanel items_;
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter headHost_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::Border itemsHost_{nullptr};
    bool expanded_ = false;

    winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> expanded_event_;
    winrt::event<winrt::Windows::Foundation::EventHandler<winrt::Windows::Foundation::IInspectable>> collapsed_event_;
};

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

void SettingsExpander::remove_onExpanding(EventToken& token) const {
    core_of(get<&Impl::contentControl_>())->Expanded(impl::to_winrt(impl::take_token(token)));
}

EventToken SettingsExpander::add_onCollapsed(EventHandler<Object> const& handler) const {
    return impl::from_winrt(core_of(get<&Impl::contentControl_>())->Collapsed({
        [handler](auto const& sender, auto const& args) {
            auto view = Object::Impl::wrap<Object>(args);
            handler(Object::Impl::wrap<Object>(sender), view);
        }}));
}

void SettingsExpander::remove_onCollapsed(EventToken& token) const {
    core_of(get<&Impl::contentControl_>())->Collapsed(impl::to_winrt(impl::take_token(token)));
}

}  // namespace wxl
