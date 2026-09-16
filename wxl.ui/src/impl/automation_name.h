#pragma once

#include <winrt/Microsoft.UI.Xaml.h>

#include <string_view>

#include "../string_param.h"

// What stands behind `automationName = L"..."` on any element.
//
// The name a screen reader speaks, and nothing else -- the half of toolTip
// that carries no hover hint. What needs it is the element whose face is
// already words but not text: a Button's accessible name comes from its
// content only when that content is a string, so a list row built out of a
// panel is nameless to automation however much text it shows. A hover hint
// on such a row would only repeat what is already on screen, which is why
// this is a property of its own rather than a use of toolTip.
//
// Private: the element arrives as the projection type the wrapper already
// holds, so this header is one only wxl's own sources ever include.

namespace wxl::impl {

void set_automation_name(winrt::Microsoft::UI::Xaml::UIElement const& element,
                         string_param text);

}  // namespace wxl::impl
