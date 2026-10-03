#pragma once

// wxl::boundTemplate -- a DataTemplate whose root is a control and whose properties are made from the item.
//
//     NavigationView {menuItemsSource = ..., menuItemTemplate = boundTemplate(u"NavigationViewItem", {
//         {u"Content", [](Object const& item) { return stringBox(name(item)); }},
//         {u"Icon", [](Object const& item) { return SymbolIcon {...}; }}})}
//
// Some controls take their items only through a DataTemplate whose root is their own item type -- a
// NavigationViewItem, a TabViewItem, a BreadcrumbBarItem -- so the function of ItemBuilder, which makes the
// element shown *in* a container, is not enough: the container itself has to come from the template. A template
// is XAML text and its properties are bindings; the bindings here go through converters that call the functions,
// so the data can stay C++ objects, with no properties for XAML to read.
//
// `root` is the name of a control of the framework's XAML namespace. A property is a name as XAML writes it,
// including the attached ones: u"ToolTipService.ToolTip". The function gets what the control hands out as the
// item (the Object put in the items source) and returns the value of the property: a string for a text, an
// element for an Icon, an IconSource for an IconSource.

#include <functional>
#include <string>
#include <vector>

#include "Object.h"
#include "generated/Microsoft.UI.Xaml.h"
#include "hstring_param.h"

namespace wxl {

struct Bound {
    std::u16string property;
    std::function<Object(Object const& item)> make;
};

DataTemplate boundTemplate(hstring_param const& root, std::vector<Bound> bindings);

}  // namespace wxl
