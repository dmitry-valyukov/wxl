#include "element_helpers.h"

#include "conversions.h"

#include "../Object.impl.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>
#include <wxl/Microsoft.UI.Xaml.Controls.impl.h>

#include <winrt/Microsoft.UI.Input.h>
#include <winrt/Microsoft.UI.Xaml.Automation.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.Input.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.System.h>

namespace wxl::impl {

namespace xaml = winrt::Microsoft::UI::Xaml;

namespace {

void pass_wheel_up(winrt::Windows::Foundation::IInspectable const& sender,
                   xaml::Input::PointerRoutedEventArgs const& args) {
    auto const viewer = sender.as<xaml::Controls::ScrollViewer>();
    auto const properties = args.GetCurrentPoint(viewer).Properties();
    if (properties.IsHorizontalMouseWheel() ||
        args.KeyModifiers() != winrt::Windows::System::VirtualKeyModifiers::None ||
        viewer.ScrollableHeight() > 0) {
        return;
    }

    // The viewer has marked the wheel handled, and a handled event is not offered
    // to the elements above it. Taking the mark back sends it on: the panel
    // that scrolls the page gets the wheel as it would over any other element,
    // and scrolls it with its own inertia.
    args.Handled(false);
}

}  // namespace

void set_cursor(winrt::Microsoft::UI::Xaml::UIElement const& element,
                InputSystemCursorShape const shape) {
    // as<>() rather than try_as<>(): an element whose object does not answer
    // for IUIElementProtected is a broken runtime, not a case to handle.
    element.as<winrt::Microsoft::UI::Xaml::IUIElementProtected>().ProtectedCursor(
        winrt::Microsoft::UI::Input::InputSystemCursor::Create(
            static_cast<winrt::Microsoft::UI::Input::InputSystemCursorShape>(shape)));
}

void set_tool_tip(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& name) {
    winrt::hstring const& value = to_winrt(name);
    winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(element,
                                                                     winrt::box_value(value));
    winrt::Microsoft::UI::Xaml::Automation::AutomationProperties::SetName(element, value);
}

void set_tool_tip_element(winrt::Microsoft::UI::Xaml::UIElement const& element,
                          ToolTip const& tip) {
    winrt::Microsoft::UI::Xaml::Controls::ToolTipService::SetToolTip(
        element, *Object::Impl::get_typed<ToolTip>(tip));
}

void set_wheel_to_parent(xaml::Controls::ScrollViewer const& viewer, bool on) {
    if (!on) {
        return;
    }
    // handledEventsToo: the viewer marks the wheel handled itself, which is
    // the whole trouble.
    viewer.AddHandler(xaml::UIElement::PointerWheelChangedEvent(),
                      winrt::box_value(xaml::Input::PointerEventHandler(&pass_wheel_up)), true);
}

void set_fluent_symbol(winrt::Microsoft::UI::Xaml::Controls::SymbolIcon const& icon,
                       FluentSymbol glyph) {
    // Both enumerations are int32_t code points into the same font, and the
    // ranges do not even meet -- Symbol sits in E1xx, FluentSymbol from E700
    // up -- so nothing is being reinterpreted here beyond the name.
    icon.Symbol(static_cast<winrt::Microsoft::UI::Xaml::Controls::Symbol>(glyph));
}

namespace {

void insert(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, Resource const& entry) {
    winrt::Windows::Foundation::IInspectable boxed = std::visit(
        [](auto const& value) -> winrt::Windows::Foundation::IInspectable {
            using type = std::remove_cvref_t<decltype(value)>;
            if constexpr (std::same_as<type, bool> || std::same_as<type, double>) {
                return winrt::box_value(value);
            } else if constexpr (std::same_as<type, HorizontalAlignment>) {
                return winrt::box_value(static_cast<winrt::Microsoft::UI::Xaml::HorizontalAlignment>(value));
            } else if constexpr (std::same_as<type, Object>) {
                winrt::Windows::Foundation::IInspectable native{nullptr};
                winrt::copy_from_abi(native, value.get_abi());
                return native;
            } else {
                return winrt::box_value(to_winrt(value));
            }
        },
        entry.value);
    dictionary.Insert(winrt::box_value(to_winrt(entry.key)), boxed);
}

}  // namespace

void add_resource(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, Resource const& entry) {
    insert(dictionary, entry);
}

void add_theme_dictionary(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary, ThemeDictionary const& theme) {
    winrt::Microsoft::UI::Xaml::ResourceDictionary inner;
    for (auto const& entry : theme.entries) {
        insert(inner, entry);
    }
    dictionary.ThemeDictionaries().Insert(winrt::box_value(to_winrt(theme.theme)), inner);
}

}  // namespace wxl::impl
