#pragma once

// What stands behind `autoSuggestBox = ...` on a NavigationView: the search box of the pane, which the framework
// lets be none. A wrapper cannot be empty, so the property takes a core::nullable and an empty one clears the box.
//
// Private: names winrt:: types.

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "../core.h"
#include <wxl/Microsoft.UI.Xaml.Controls.h>

namespace wxl::impl {

void set_auto_suggest_box(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& view, core::nullable<AutoSuggestBox> const& box);

}  // namespace wxl::impl
