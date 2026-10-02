#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

// What stands behind `elementItems = true` on a TreeView: its items are elements, and an item is shown
// as the element it is.
//
// A TreeView draws the content of a node through ItemTemplate, and with no template it writes the
// name of the type. A template is a DataTemplate, and that kind of template is made from XAML text
// only; the shortest one that does what is wanted here is a ContentControl that shows the content of its node.
//
// Private: the control arrives as the projection type the wrapper already holds.

namespace wxl::impl {

void set_element_items(winrt::Microsoft::UI::Xaml::Controls::TreeView const& control, bool on);

}  // namespace wxl::impl
