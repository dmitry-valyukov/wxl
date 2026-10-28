#pragma once

// What stands behind the properties wxl gives any element, or a control of one kind, that WinUI
// has no plain property for.
//
// Private: the element arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.UI.Xaml.h>
#include <winrt/Windows.Foundation.h>

#include "../Resource.h"
#include "../hstring_param.h"
#include <wxl/FluentSymbol.h>
#include <wxl/Microsoft.UI.Input.Enums.h>

namespace wxl {
class ToolTip;
}

namespace wxl::impl {

/// `cursor = InputSystemCursorShape::SizeWestEast`: the shape the pointer takes over the element,
/// for the element's lifetime. WinUI keeps it on UIElement.ProtectedCursor; "protected" is a word
/// for projections, not for the ABI -- any element's object answers QueryInterface for
/// IUIElementProtected, so no subclass is needed.
void set_cursor(winrt::Microsoft::UI::Xaml::UIElement const& element, InputSystemCursorShape shape);

/// `toolTip = u"..."`: one text, two homes -- ToolTipService, the hover hint, and
/// AutomationProperties.Name, the name a screen reader speaks. The property exists for controls
/// whose face is a glyph rather than a word, and such a control needs naming in both places.
void set_tool_tip(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text);

/// The tip as an element of its own -- content that is not text, a placement, an offset. Only
/// the hover half: an element has no words to give a screen reader.
void set_tool_tip_element(winrt::Microsoft::UI::Xaml::UIElement const& element, ToolTip const& tip);

/// `wheelToParent = true` on a ScrollViewer. A viewer takes the wheel and marks it handled even
/// when it has nothing to scroll along it, so a horizontal code block stops the page around it.
/// With this on, a wheel turn the viewer cannot use -- no scrollable height, no modifier key held
/// -- goes on up the tree to the panel that scrolls the page. Added once, when the viewer is
/// built; false adds nothing.
void set_wheel_to_parent(winrt::Microsoft::UI::Xaml::Controls::ScrollViewer const& viewer, bool on);

/// The second spelling of SymbolIcon's Symbol: a FluentSymbol, every name the font's own
/// documentation carries, where WinRT's Symbol has 197 names from Windows 8. SymbolIcon takes a
/// code point in SymbolThemeFontFamily and nothing else, so the crossing is a cast, written here
/// once rather than at every call site.
void set_fluent_symbol(winrt::Microsoft::UI::Xaml::Controls::SymbolIcon const& icon,
                       FluentSymbol glyph);

/// What the header and the description of a settings card, and the header of a headered
/// control, count as "no text": null or an empty string -- the IsNullOrEmpty of the Toolkit's
/// SettingsCard, which then collapses the part.
inline bool is_blank(winrt::Windows::Foundation::IInspectable const& content) {
    if (!content) {
        return true;
    }
    if (auto const value = content.try_as<winrt::Windows::Foundation::IPropertyValue>()) {
        return value.Type() == winrt::Windows::Foundation::PropertyType::String &&
               value.GetString().empty();
    }
    return false;
}

// What stands behind `entry = Resource {...}` on a ResourceDictionary: the
// value, boxed as the framework's own type, under the key. An entry with a key
// that is already there replaces it, as `Insert` on the dictionary does.
void add_resource(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary,
                  Resource const& entry);

// The same under a theme: the entries go in a dictionary of their own that ThemeDictionaries keeps under the theme's name.
void add_theme_dictionary(winrt::Microsoft::UI::Xaml::ResourceDictionary const& dictionary,
                          ThemeDictionary const& theme);

}  // namespace wxl::impl
