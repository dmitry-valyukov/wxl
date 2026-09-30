#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include "../hstring_param.h"

// What stands behind `toolTip = L"..."` on any element.
//
// One text, two homes: the same words go to ToolTipService -- the hover
// hint -- and to AutomationProperties.Name, the name a screen reader
// speaks. They are set together deliberately: the property exists for
// controls whose face is a glyph rather than a word, and such a control
// needs naming in both places or it is nameless in one of them.
//
// Private: the element arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl {
class ToolTip;
}

namespace wxl::impl {

void set_tool_tip(winrt::Microsoft::UI::Xaml::UIElement const& element, hstring_param const& text);

// The tip as an element of its own -- content that is not text, a placement,
// an offset. Only the hover half: an element has no words to give a screen
// reader, which is what toolTip = "..." adds on top.
void set_tool_tip_element(winrt::Microsoft::UI::Xaml::UIElement const& element,
                          ToolTip const& tip);

}  // namespace wxl::impl
