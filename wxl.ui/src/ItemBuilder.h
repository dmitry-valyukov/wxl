#pragma once

// wxl::ItemBuilder -- an item template written as a function.
//
//     ListView {itemsSource = ..., itemTemplate = [](Object const& item) { return TextBlock {...}; }}
//
// A DataTemplate of XAML is text loaded by the framework and bound to the properties of the data
// object; neither exists here, where the data are C++ objects and the element is made by code. The
// function takes what the control hands out as the item (the Object put in ItemsSource, which the
// application unpacks the way it packed it: intOf, stringOf) and returns the element to show for it.
//
// A control that realises its containers as they scroll into view (ListView, GridView) calls the
// function for those containers only, and again when a container is reused for another item, so a
// list of a hundred thousand items costs the elements of one screen.

#include <functional>

#include "Object.h"
#include "generated/Microsoft.UI.Xaml.h"

namespace wxl {

using ItemBuilder = std::function<UIElement(Object const& item)>;

}  // namespace wxl
