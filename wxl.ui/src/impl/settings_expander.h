#pragma once

// The control behind wxl::SettingsExpander: a SettingsCard that opens onto a list of cards. The head is a SettingsCard of
// its own, made here and put into the template; a click on it opens and closes the list, and its action icon is the chevron
// that says which. The cards of the list are dressed flat -- a hairline between them, square, on the second card brush --
// when they are put in, and again whenever the list changes.
//
// Private: the projection types are used freely, so this header is one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "settings_card.h"

namespace wxl::impl {

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

}  // namespace wxl::impl
