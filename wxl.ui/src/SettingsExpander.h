#pragma once

// wxl::SettingsExpander -- a SettingsCard that opens onto more of them, as the Toolkit's does: the head is a card of its
// own (header, description, icon, the setting at the right) and a click on it opens the list; the cards put in the list
// are dressed flat and read as the parts of one.
//
//     SettingsExpander {
//         header = u"Sound",
//         description = u"Controls provide audible feedback",
//         headerIcon = FontIcon {glyph = u""},
//         ToggleSwitch {},                                 // the setting of the head: the content
//         items[SettingsCard {header = u"Enable Spatial Audio", ToggleSwitch {}}],
//     }
//
//   header, description, headerIcon   the head, as SettingsCard has them
//   content (the unnamed child)       the setting at the right of the head
//   items[...]                        the cards under it; cards are dressed flat, any other element is left as it is
//   isExpanded                        whether the list is open
//   onExpanding, onCollapsed          the list opened, the list closed
//   isEnabled                         the head is dimmed with it
//
// A ContentControl wxl derives from, like SettingsCard: the head and the list live in the object and its template.

#include "SettingsCard.h"
#include <wxl/Members.h>
#include <wxl/Microsoft.UI.Xaml.Controls.h>

namespace wxl {

class SettingsExpander : public ContentControl {
public:
    SettingsExpander();

    template <typename... Setters>
        requires impl::setter_pack<SettingsExpander, Setters...>
    explicit SettingsExpander(Setters&&... setters) : SettingsExpander() {
        (impl::apply_argument(*this, std::forward<Setters>(setters)), ...);
    }

    void content(Object const& value) const;
    Object content() const;
    void header(Object const& value) const;
    void header(hstring_param const& value) const;
    void description(Object const& value) const;
    void description(hstring_param const& value) const;
    void headerIcon(IconElement const& value) const;
    Collection<UIElement> items() const;
    bool isExpanded() const;
    void isExpanded(bool value) const;

    EventToken add_onExpanding(EventHandler<Object> const& handler) const;
    void remove_onExpanding(EventToken token) const;
    EventToken add_onCollapsed(EventHandler<Object> const& handler) const;
    void remove_onCollapsed(EventToken token) const;

    void setPositional(UIElement const& element) const { content(element); }

protected:
    explicit SettingsExpander(Impl* impl) noexcept : ContentControl{impl} {}

    friend class Object::Impl;
};

}  // namespace wxl
