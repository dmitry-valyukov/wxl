#pragma once

#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>

#include "../ItemBuilder.h"
#include "../ItemContainerPreset.h"
#include "../Thickness.h"

// What stands behind `itemTemplate = [](Object const& item) {...}` on a ListView or GridView.
//
// The two controls make a container for an item when it comes into view and give the application a
// moment to fill it (ContainerContentChanging): the container's content is set to the element the
// function made for the item, and cleared when the container goes to the recycle queue. That is
// what an ItemTemplate does, with the function in place of the template.
//
// Private: the control arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

namespace wxl::impl {

void set_item_template(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list, ItemBuilder const& build);

/// The margin of every container of the list, the ones on screen and the ones made later -- what an
/// ItemContainerStyle with a Setter for Margin says.
void set_item_margin(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list, Thickness const& margin);

/// The preset every container of the list wears, once, the first time the control prepares it (ContainerContentChanging) -- what an
/// ItemContainerStyle says. The containers on screen already are dressed here and now.
void set_item_container_style(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list, ItemContainerPreset const& preset);

}  // namespace wxl::impl
