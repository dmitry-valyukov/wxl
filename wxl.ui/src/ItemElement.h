#pragma once

// wxl::itemElement -- the element that the item template of a ListView or GridView made for an item.
//
// The element an application's function (itemTemplate = [](Object const& item) {...}) returns is what
// the item is shown as; this is the way back to it for the application that wants to do something to
// it later -- start a connected animation from it, scroll to it, focus it. The framework's own way
// to the elements of a template is by name through the control; the function made them in code and
// can name them no more than it can keep them.

#include "generated/Microsoft.UI.Xaml.Controls.h"

namespace wxl {

/// The element made for `item`, or an empty one when the item has no container in view -- a container
/// is made when the item comes into view and goes back to the queue when it leaves it.
UIElement itemElement(ListViewBase const& list, Object const& item);

}  // namespace wxl
