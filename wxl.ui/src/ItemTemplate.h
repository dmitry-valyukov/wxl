#pragma once

// An item template written as code: the function that makes an item's element (ItemBuilder), the
// DataTemplate whose root is a control and whose properties are made from the item
// (boundTemplate), and the way back from an item to the element its template made (itemElement).
//
// Included by the generated wrappers of the list controls (the profile's cppInclude), so it names
// no type of Microsoft.UI.Xaml.Controls but declares the one it needs: the generated header of
// that namespace is the one that includes this.

#include <functional>
#include <string>
#include <vector>

#include "Object.h"
#include <wxl/Microsoft.UI.Xaml.h>
#include "hstring_param.h"

namespace wxl {

class ListViewBase;

/// An item template written as a function.
///
///     ListView {itemsSource = ..., itemTemplate = [](Object const& item) { return TextBlock {...}; }}
///
/// A DataTemplate of XAML is text loaded by the framework and bound to the properties of the data
/// object; neither exists here, where the data are C++ objects and the element is made by code.
/// The function takes what the control hands out as the item (the Object put in ItemsSource,
/// which the application unpacks the way it packed it: unboxValue<int64_t>, unboxValue<hstring>)
/// and returns the element to show for it.
///
/// A control that realises its containers as they scroll into view (ListView, GridView) calls the
/// function for those containers only, and again when a container is reused for another item, so
/// a list of a hundred thousand items costs the elements of one screen.
using ItemBuilder = std::function<UIElement(Object const& item)>;

/// One property of a bound template: its name as XAML writes it, attached ones included
/// (u"ToolTipService.ToolTip"), and the function that makes its value from the item -- a string
/// for a text, an element for an Icon, an IconSource for an IconSource.
struct BoundProperty {
    std::u16string property;
    std::function<Object(Object const& item)> make;
};

/// A DataTemplate whose root is a control and whose properties are made from the item.
///
///     NavigationView {menuItemsSource = ..., menuItemTemplate = boundTemplate(u"NavigationViewItem", {
///         {u"Content", [](Object const& item) { return boxValue(name(item)); }},
///         {u"Icon", [](Object const& item) { return SymbolIcon {...}; }}})}
///
/// Some controls take their items only through a DataTemplate whose root is their own item type --
/// a NavigationViewItem, a TabViewItem, a BreadcrumbBarItem -- so an ItemBuilder, which makes the
/// element shown *in* a container, is not enough: the container itself has to come from the
/// template. A template is XAML text and its properties are bindings; the bindings here go through
/// converters that call the functions, so the data can stay C++ objects, with no properties for
/// XAML to read. `root` is the name of a control of the framework's XAML namespace.
DataTemplate boundTemplate(hstring_param const& root, std::vector<BoundProperty> bindings);

/// The element that the item template of a ListView or GridView made for `item`, or nothing when
/// the item has no container in view -- a container is made when the item comes into view and
/// goes back to the queue when it leaves it. The element an application's function returns is what
/// the item is shown as; this is the way back to it, to do something to it later -- start a
/// connected animation from it, scroll to it, focus it.
core::nullable<UIElement> itemElement(ListViewBase const& list, Object const& item);

}  // namespace wxl
