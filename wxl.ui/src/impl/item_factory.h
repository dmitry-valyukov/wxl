#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.h>

#include "../ItemBuilder.h"

// What stands behind `itemTemplate = [](Object const& item) {...}` on an ItemsRepeater or an ItemsView,
// and behind `itemsSource = BindOutput{list, build}` on them (attach_items in bound_list.h).
//
// These two controls make an element for an item through a factory (IElementFactory): they hand it the
// item and take the element back. A factory of the framework is made from a DataTemplate -- XAML
// text -- or is a class an application writes; this one is the object of that class with the function
// handed in, the same way CustomLayout is the object of a layout class with its two functions handed in.
// It is a COM object of wxl's own (impl/sta_com.h).
//
// The function is called inside a binding_scope the factory keeps for the element. An element given back
// (RecycleElement) has its bindings cut at once, and is taken out of the control's children the next
// time the control asks for an element.
//
// Private: the control arrives as the projection type the wrapper already holds, so this header is one
// only wxl's own sources ever include.

namespace wxl::impl {

void set_repeater_template(winrt::Microsoft::UI::Xaml::Controls::ItemsRepeater const& repeater, ItemBuilder const& build);
void set_items_view_template(winrt::Microsoft::UI::Xaml::Controls::ItemsView const& view, ItemBuilder const& build);

}  // namespace wxl::impl
