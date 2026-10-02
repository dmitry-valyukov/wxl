#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "../generated/Microsoft.UI.Xaml.Controls.Enums.h"

// What stands behind `itemsPanelOrientation = Orientation::Vertical` on an ItemsControl.
//
// ItemsControl.ItemsPanel takes an ItemsPanelTemplate, and that kind of template is made from XAML
// text only: nothing in the API builds one from objects. The template here is the shortest text
// there is -- a VirtualizingStackPanel -- run through the framework's own loader, so the control gets
// the very template a XAML file would have given it.
//
// Private: the control arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

namespace wxl::impl {

void set_items_panel_orientation(winrt::Microsoft::UI::Xaml::Controls::ItemsControl const& control, Orientation orientation);

}  // namespace wxl::impl
