#include "settings_card.h"
#include "blank_content.h"

#include <winrt/Microsoft.UI.Xaml.Markup.h>
#include <winrt/Windows.System.h>

namespace wxl::impl {

namespace xaml = winrt::Microsoft::UI::Xaml;
namespace controls = winrt::Microsoft::UI::Xaml::Controls;

namespace {

// The look of the card: the framework's own card brushes, the states of a control that is clicked, the parts the code
// finds by name. Written as markup because a template is -- the parts are named in it.
constexpr wchar_t const* templateText =
    L"<ControlTemplate TargetType='ContentControl' xmlns='http://schemas.microsoft.com/winfx/2006/xaml/presentation' "
    L"xmlns:x='http://schemas.microsoft.com/winfx/2006/xaml'>"
    // The surface: what a card is a card by. The states of the second group change it, so a card in an expander is
    // flat and a card that heads an open expander is square below.
    L"<Border x:Name='PART_Frame' CornerRadius='4' BorderThickness='1' "
    L"Background='{ThemeResource CardBackgroundFillColorDefaultBrush}' BorderBrush='{ThemeResource CardStrokeColorDefaultBrush}'>"
    L"<Grid x:Name='PART_Root' MinHeight='68' Padding='16,12' Background='Transparent' CornerRadius='4'>"
    L"<VisualStateManager.VisualStateGroups>"
    L"<VisualStateGroup x:Name='CommonStates'>"
    L"<VisualState x:Name='Normal'/>"
    L"<VisualState x:Name='PointerOver'><VisualState.Setters>"
    L"<Setter Target='PART_Root.Background' Value='{ThemeResource ControlFillColorSecondaryBrush}'/>"
    L"</VisualState.Setters></VisualState>"
    L"<VisualState x:Name='Pressed'><VisualState.Setters>"
    L"<Setter Target='PART_Root.Background' Value='{ThemeResource ControlFillColorTertiaryBrush}'/>"
    L"</VisualState.Setters></VisualState>"
    L"<VisualState x:Name='Disabled'><VisualState.Setters>"
    L"<Setter Target='PART_Root.Background' Value='{ThemeResource ControlFillColorDisabledBrush}'/>"
    L"<Setter Target='PART_Header.Foreground' Value='{ThemeResource TextFillColorDisabledBrush}'/>"
    L"<Setter Target='PART_Description.Foreground' Value='{ThemeResource TextFillColorDisabledBrush}'/>"
    L"</VisualState.Setters></VisualState>"
    L"</VisualStateGroup>"
    L"<VisualStateGroup x:Name='SurfaceStates'>"
    L"<VisualState x:Name='Standalone'/>"
    L"<VisualState x:Name='Flat'><VisualState.Setters>"
    L"<Setter Target='PART_Frame.CornerRadius' Value='0'/>"
    L"<Setter Target='PART_Root.CornerRadius' Value='0'/>"
    L"<Setter Target='PART_Frame.BorderThickness' Value='0,1,0,0'/>"
    L"<Setter Target='PART_Frame.Background' Value='{ThemeResource CardBackgroundFillColorSecondaryBrush}'/>"
    L"</VisualState.Setters></VisualState>"
    L"<VisualState x:Name='OpenHeader'><VisualState.Setters>"
    L"<Setter Target='PART_Frame.CornerRadius' Value='4,4,0,0'/>"
    L"<Setter Target='PART_Root.CornerRadius' Value='4,4,0,0'/>"
    L"</VisualState.Setters></VisualState>"
    L"</VisualStateGroup>"
    L"</VisualStateManager.VisualStateGroups>"
    L"<Grid.ColumnDefinitions><ColumnDefinition Width='Auto'/><ColumnDefinition Width='*'/><ColumnDefinition Width='Auto'/>"
    L"<ColumnDefinition Width='Auto'/></Grid.ColumnDefinitions>"
    L"<Grid.RowDefinitions><RowDefinition Height='Auto'/><RowDefinition Height='Auto'/></Grid.RowDefinitions>"
    L"<ContentPresenter x:Name='PART_HeaderIcon' Grid.RowSpan='2' VerticalAlignment='Center' Margin='0,0,16,0' Visibility='Collapsed'/>"
    L"<StackPanel x:Name='PART_Text' Grid.Column='1' Grid.RowSpan='2' VerticalAlignment='Center'>"
    L"<ContentPresenter x:Name='PART_Header' TextWrapping='WrapWholeWords' Visibility='Collapsed'/>"
    L"<ContentPresenter x:Name='PART_Description' FontSize='12' TextWrapping='WrapWholeWords' "
    L"Foreground='{ThemeResource TextFillColorSecondaryBrush}' Visibility='Collapsed'/>"
    L"</StackPanel>"
    L"<ContentPresenter x:Name='PART_Content' Grid.Column='2' Grid.RowSpan='2' VerticalAlignment='Center' HorizontalAlignment='Right' "
    L"Content='{TemplateBinding Content}' ContentTemplate='{TemplateBinding ContentTemplate}'/>"
    L"<ContentPresenter x:Name='PART_ActionIcon' Grid.Column='3' Grid.RowSpan='2' VerticalAlignment='Center' Margin='16,0,0,0' Visibility='Collapsed'/>"
    L"</Grid></Border></ControlTemplate>";

// Enter and Space click a card as they click a button.
bool is_click_key(winrt::Windows::System::VirtualKey key) {
    return key == winrt::Windows::System::VirtualKey::Enter || key == winrt::Windows::System::VirtualKey::Space;
}

}  // namespace

SettingsCardCore::SettingsCardCore() {
    Template(xaml::Markup::XamlReader::Load(templateText).as<controls::ControlTemplate>());
    IsEnabledChanged([this](auto const&, auto const&) { go_to_state(); });
}

void SettingsCardCore::OnApplyTemplate() {
    xaml::IFrameworkElementOverridesT<SettingsCardCore>::OnApplyTemplate();

    root_ = GetTemplateChild(L"PART_Root").try_as<controls::Grid>();
    headerIconPart_ = GetTemplateChild(L"PART_HeaderIcon").try_as<controls::ContentPresenter>();
    headerPart_ = GetTemplateChild(L"PART_Header").try_as<controls::ContentPresenter>();
    descriptionPart_ = GetTemplateChild(L"PART_Description").try_as<controls::ContentPresenter>();
    contentPart_ = GetTemplateChild(L"PART_Content").try_as<controls::ContentPresenter>();
    actionIconPart_ = GetTemplateChild(L"PART_ActionIcon").try_as<controls::ContentPresenter>();
    textPart_ = GetTemplateChild(L"PART_Text").try_as<controls::StackPanel>();
    apply();
    go_to_state();
    go_to_surface();
}

void SettingsCardCore::apply() {
    auto const show = [](controls::ContentPresenter const& part, winrt::Windows::Foundation::IInspectable const& content) {
        if (!part) {
            return;
        }
        // Null and an empty string are the same thing here -- no text -- and the part is collapsed.
        bool const blank = is_blank(content);
        part.Content(blank ? winrt::Windows::Foundation::IInspectable{nullptr} : content);
        part.Visibility(blank ? xaml::Visibility::Collapsed : xaml::Visibility::Visible);
    };
    show(headerIconPart_, headerIcon_);
    show(headerPart_, header_);
    show(descriptionPart_, description_);

    if (actionIconPart_) {
        bool const visible = isActionIconVisible();
        winrt::Windows::Foundation::IInspectable icon = actionIcon_;
        if (!icon && visible) {
            controls::FontIcon chevron;
            chevron.Glyph(L"");
            chevron.FontSize(12);
            icon = chevron;
        }
        actionIconPart_.Content(icon);
        actionIconPart_.Visibility(visible ? xaml::Visibility::Visible : xaml::Visibility::Collapsed);
    }

    // Where the setting stands: right of the text across both rows, or in the second row under it.
    if (textPart_) {
        controls::Grid::SetRowSpan(textPart_, alignment_ == SettingsCardContentAlignment::Right ? 2 : 1);
    }
    if (contentPart_) {
        if (alignment_ == SettingsCardContentAlignment::Right) {
            controls::Grid::SetColumn(contentPart_, 2);
            controls::Grid::SetRow(contentPart_, 0);
            controls::Grid::SetRowSpan(contentPart_, 2);
            controls::Grid::SetColumnSpan(contentPart_, 1);
            contentPart_.HorizontalAlignment(xaml::HorizontalAlignment::Right);
            contentPart_.Margin(xaml::Thickness{16, 0, 0, 0});
        } else {
            controls::Grid::SetColumn(contentPart_, 1);
            controls::Grid::SetRow(contentPart_, 1);
            controls::Grid::SetRowSpan(contentPart_, 1);
            controls::Grid::SetColumnSpan(contentPart_, 1);
            contentPart_.HorizontalAlignment(alignment_ == SettingsCardContentAlignment::Left ? xaml::HorizontalAlignment::Left
                                                                                               : xaml::HorizontalAlignment::Stretch);
            contentPart_.Margin(xaml::Thickness{0, 8, 0, 0});
        }
    }
}

void SettingsCardCore::go_to_state() {
    wchar_t const* state = L"Normal";
    if (!IsEnabled()) {
        state = L"Disabled";
    } else if (clickable_) {
        state = pressed_ ? L"Pressed" : (hover_ ? L"PointerOver" : L"Normal");
    }
    xaml::VisualStateManager::GoToState(this->try_as<controls::Control>(), state, true);
}

void SettingsCardCore::surface(Surface value) {
    surface_ = value;
    go_to_surface();
}

void SettingsCardCore::go_to_surface() {
    wchar_t const* state = surface_ == Surface::Flat ? L"Flat" : (surface_ == Surface::OpenHeader ? L"OpenHeader" : L"Standalone");
    xaml::VisualStateManager::GoToState(this->try_as<controls::Control>(), state, false);
}

void SettingsCardCore::raise_click() {
    click_(this->try_as<winrt::Windows::Foundation::IInspectable>(), xaml::RoutedEventArgs{});
}

void SettingsCardCore::header(winrt::Windows::Foundation::IInspectable const& value) {
    header_ = value;
    apply();
}

void SettingsCardCore::description(winrt::Windows::Foundation::IInspectable const& value) {
    description_ = value;
    apply();
}

void SettingsCardCore::headerIcon(winrt::Microsoft::UI::Xaml::UIElement const& value) {
    headerIcon_ = value;
    apply();
}

void SettingsCardCore::actionIcon(winrt::Microsoft::UI::Xaml::UIElement const& value) {
    actionIcon_ = value;
    apply();
}

void SettingsCardCore::isClickEnabled(bool value) {
    clickable_ = value;
    IsTabStop(value);
    UseSystemFocusVisuals(value);
    apply();
    go_to_state();
}

void SettingsCardCore::isActionIconVisible(bool value) {
    actionIconVisible_ = value ? Tri::Yes : Tri::No;
    apply();
}

void SettingsCardCore::contentAlignment(SettingsCardContentAlignment value) {
    alignment_ = value;
    apply();
}

void SettingsCardCore::OnPointerEntered(xaml::Input::PointerRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnPointerEntered(args);
    hover_ = true;
    go_to_state();
}

void SettingsCardCore::OnPointerExited(xaml::Input::PointerRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnPointerExited(args);
    hover_ = false;
    pressed_ = false;
    go_to_state();
}

void SettingsCardCore::OnPointerPressed(xaml::Input::PointerRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnPointerPressed(args);
    if (clickable_ && IsEnabled()) {
        pressed_ = true;
        CapturePointer(args.Pointer());
        go_to_state();
    }
}

void SettingsCardCore::OnPointerReleased(xaml::Input::PointerRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnPointerReleased(args);
    bool const clicked = pressed_ && hover_;
    pressed_ = false;
    ReleasePointerCapture(args.Pointer());
    go_to_state();
    if (clicked) {
        raise_click();
    }
}

void SettingsCardCore::OnPointerCaptureLost(xaml::Input::PointerRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnPointerCaptureLost(args);
    pressed_ = false;
    go_to_state();
}

void SettingsCardCore::OnKeyDown(xaml::Input::KeyRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnKeyDown(args);
    if (clickable_ && IsEnabled() && is_click_key(args.Key())) {
        pressed_ = true;
        args.Handled(true);
        go_to_state();
    }
}

void SettingsCardCore::OnKeyUp(xaml::Input::KeyRoutedEventArgs const& args) {
    controls::IControlOverridesT<SettingsCardCore>::OnKeyUp(args);
    if (pressed_ && is_click_key(args.Key())) {
        pressed_ = false;
        args.Handled(true);
        go_to_state();
        raise_click();
    }
}

}  // namespace wxl::impl
