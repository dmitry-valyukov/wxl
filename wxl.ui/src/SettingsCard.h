#pragma once

// wxl::SettingsCard -- a row of a settings page: an icon, a header with its description under it, and the setting
// itself on the right; the card of Windows Community Toolkit, which is managed code and has no projection, written here.
//
//     SettingsCard {
//         header = u"App theme",
//         description = u"Select which app theme to display",
//         headerIcon = FontIcon {glyph = u""},
//         ComboBox {...},                      // the setting: the content of the card
//     }
//
// A ContentControl that wxl derives from -- the first control of the library to carry state of its own. The header,
// the description, the icons live in the object, and the template that shows them is part of it. The derivation is COM
// aggregation (impl/settings_card.h), and a handler that names SettingsCard as its sender reaches the same object.
//
//   content            what stands right of the text (or under it, see contentAlignment); the unnamed child
//   header             the name of the setting: text, or an element
//   description        a line under the name: text, or an element (a link, say)
//   headerIcon         an icon left of the text
//   actionIcon         an icon right of the setting; a clickable card shows a chevron when it has none
//   isClickEnabled     the whole card is a button: pointer, Enter and Space click it, and the card answers to them
//   isActionIconVisible  whether the action icon is shown; follows isClickEnabled until it is written
//   contentAlignment   Right (the default), Left, Vertical: where the content stands
//   isEnabled          a disabled card is dimmed
//   onClick            the click of a clickable card
//
// The tags are written qualified inside a subclass -- `dsl::header` -- for the reason Card.h gives.

#include "SettingsCardAlignment.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Members.h>

namespace wxl {

class SettingsCard : public ContentControl {
public:
    SettingsCard();

    template <typename... Setters>
        requires impl::setter_pack<SettingsCard, Setters...>
    explicit SettingsCard(Setters&&... setters) : SettingsCard() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    void header(Object const& value) const;
    void header(hstring_param const& value) const;
    Object header() const;
    void description(Object const& value) const;
    void description(hstring_param const& value) const;
    Object description() const;
    void headerIcon(IconElement const& value) const;
    void actionIcon(IconElement const& value) const;
    bool isClickEnabled() const;
    void isClickEnabled(bool value) const;
    bool isActionIconVisible() const;
    void isActionIconVisible(bool value) const;
    SettingsCardContentAlignment contentAlignment() const;
    void contentAlignment(SettingsCardContentAlignment value) const;

    EventToken add_onClick(EventHandler<RoutedEventArgs> const& handler) const;
    void remove_onClick(EventToken token) const;

    using ContentControl::setPositional;
    void setPositional(UIElement const& element) const { content(element); }

protected:
    explicit SettingsCard(Impl* impl) noexcept : ContentControl{impl} {}

    friend class Object::Impl;
};

}  // namespace wxl
