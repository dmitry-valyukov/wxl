#pragma once

// The control behind wxl::SettingsCard: a ContentControl that the application's program derives from, with state of its
// own -- a header, a description, icons, whether it is clicked -- and the overrides that put that state on the parts of
// its template. The derivation is COM aggregation (the control is the inner object of an outer one that answers for
// the overrides interfaces), which here is C++/WinRT's `ContentControlT<D>`: the same CreateInstance(outer, &inner)
// that sandbox/Aggregation exercises on FrameworkElement, ContentControl and UserControl.
//
// Private: the projection types are used freely, so this header is one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "../SettingsCardAlignment.h"

namespace wxl::impl {

struct SettingsCardCore : winrt::Microsoft::UI::Xaml::Controls::ContentControlT<SettingsCardCore> {
    // How the card is dressed: on its own, flat inside an expander, or square below as the head of an open one.
    enum class Surface { Standalone, Flat, OpenHeader };
    void surface(Surface value);
    // The name by which a card is told from any other ContentControl (a runtime class that metadata does not know).
    static constexpr wchar_t const* className = L"wxl.SettingsCard";
    winrt::hstring GetRuntimeClassName() const { return className; }

    SettingsCardCore();

    // The overrides: what the control does where a plain ContentControl does nothing.
    void OnApplyTemplate();
    void OnPointerEntered(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnPointerExited(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnPointerPressed(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnPointerReleased(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnPointerCaptureLost(winrt::Microsoft::UI::Xaml::Input::PointerRoutedEventArgs const& args);
    void OnKeyDown(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);
    void OnKeyUp(winrt::Microsoft::UI::Xaml::Input::KeyRoutedEventArgs const& args);

    // The state, each setter putting it on the parts that exist (the template is applied later than the properties are set).
    winrt::Windows::Foundation::IInspectable header() const { return header_; }
    void header(winrt::Windows::Foundation::IInspectable const& value);
    winrt::Windows::Foundation::IInspectable description() const { return description_; }
    void description(winrt::Windows::Foundation::IInspectable const& value);
    winrt::Microsoft::UI::Xaml::UIElement headerIcon() const { return headerIcon_; }
    void headerIcon(winrt::Microsoft::UI::Xaml::UIElement const& value);
    winrt::Microsoft::UI::Xaml::UIElement actionIcon() const { return actionIcon_; }
    void actionIcon(winrt::Microsoft::UI::Xaml::UIElement const& value);
    bool isClickEnabled() const { return clickable_; }
    void isClickEnabled(bool value);
    bool isActionIconVisible() const { return actionIconVisible_ == Tri::Unset ? clickable_ : actionIconVisible_ == Tri::Yes; }
    void isActionIconVisible(bool value);
    SettingsCardContentAlignment contentAlignment() const { return alignment_; }
    void contentAlignment(SettingsCardContentAlignment value);

    winrt::event_token Click(winrt::Microsoft::UI::Xaml::RoutedEventHandler const& handler) { return click_.add(handler); }
    void Click(winrt::event_token const& token) noexcept { click_.remove(token); }

private:
    enum class Tri { Unset, No, Yes };

    void apply();
    void go_to_state();
    void go_to_surface();
    void raise_click();

    winrt::Windows::Foundation::IInspectable header_{nullptr};
    winrt::Windows::Foundation::IInspectable description_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement headerIcon_{nullptr};
    winrt::Microsoft::UI::Xaml::UIElement actionIcon_{nullptr};
    bool clickable_ = false;
    Tri actionIconVisible_ = Tri::Unset;
    SettingsCardContentAlignment alignment_ = SettingsCardContentAlignment::Right;

    Surface surface_ = Surface::Standalone;
    bool hover_ = false;
    bool pressed_ = false;

    // The parts of the template, found once it is applied.
    winrt::Microsoft::UI::Xaml::Controls::Grid root_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::StackPanel textPart_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter headerIconPart_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter headerPart_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter descriptionPart_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter contentPart_{nullptr};
    winrt::Microsoft::UI::Xaml::Controls::ContentPresenter actionIconPart_{nullptr};

    winrt::event<winrt::Microsoft::UI::Xaml::RoutedEventHandler> click_;
};

}  // namespace wxl::impl
