#pragma once

// What stands behind the properties of lists and item collections WinUI writes as templates of
// XAML text, or as collections with a property of their own.
//
// Private: the control arrives as the projection type the wrapper already holds, so this header is
// one only wxl's own sources ever include.

#include <winrt/Microsoft.UI.Xaml.Controls.Primitives.h>
#include <winrt/Microsoft.UI.Xaml.Controls.h>
#include <winrt/Microsoft.Windows.Storage.Pickers.h>

#include "../core.h"
#include "../ItemTemplate.h"
#include "../ItemContainerPreset.h"
#include "../FileTypeChoice.h"
#include "../Thickness.h"
#include "../hstring_param.h"
#include <wxl/Microsoft.UI.Xaml.Controls.Enums.h>
// AutoSuggestBox complete: core::nullable of an incomplete class picks the raw-pointer form and
// keeps it for the rest of the translation unit.
#include <wxl/Microsoft.UI.Xaml.Controls.h>

namespace wxl::impl {

// What makes an element for an item, both as the platform hands them over: an ItemBuilder seen from
// the WinRT side, for item_template.cpp and item_factory.cpp alike.
using winrt_item_builder = core::function<winrt::Windows::Foundation::IInspectable(
    winrt::Windows::Foundation::IInspectable const& item)>;

// `itemTemplate = [](Object const& item) {...}` on a ListView or GridView, and `itemsSource =
// BindOutput{list, build}` (attach_items in bound_list.h). The controls make a container for an
// item when it comes into view and let the application fill it (ContainerContentChanging): the
// container shows the element the function made for the item, and is cleared when it goes to the
// recycle queue -- an ItemTemplate with the function in place of the template. The function runs
// inside a binding_scope the container keeps (impl/element_scope.h), so the bindings of an element
// go when the container gives it back. The state of a list is held by the list, through its
// handler. Defined in item_template.cpp.

void set_item_template(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list,
                       ItemBuilder const& build);

/// The margin of every container of the list, the ones on screen and the ones made later -- what
/// an ItemContainerStyle with a Setter for Margin says.
void set_item_margin(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list,
                     Thickness const& margin);

/// The preset every container of the list wears, once, the first time the control prepares it
/// (ContainerContentChanging) -- what an ItemContainerStyle says. The containers on screen already
/// are dressed here and now.
void set_item_container_style(winrt::Microsoft::UI::Xaml::Controls::ListViewBase const& list,
                              ItemContainerPreset const& preset);

// `itemTemplate = [](Object const& item) {...}` on an ItemsRepeater or an ItemsView, and
// `itemsSource = BindOutput{list, build}` on them. These controls make an element for an item
// through a factory (IElementFactory); this one is a COM object of wxl's own (impl/sta_com.h) with
// the function handed in, the way CustomLayout is a layout class with its functions handed in. The
// function runs inside a binding_scope the factory keeps for the element; an element given back
// (RecycleElement) has its bindings unbound at once. Defined in item_factory.cpp.

void set_repeater_template(winrt::Microsoft::UI::Xaml::Controls::ItemsRepeater const& repeater,
                           ItemBuilder const& build);
void set_items_view_template(winrt::Microsoft::UI::Xaml::Controls::ItemsView const& view,
                             ItemBuilder const& build);

/// `itemsPanelOrientation = Orientation::Vertical` on an ItemsControl. An ItemsPanelTemplate is
/// made from XAML text only, so the shortest text there is -- a VirtualizingStackPanel -- is run
/// through the framework's own loader, and the control gets the template a XAML file would give.
void set_items_panel_orientation(winrt::Microsoft::UI::Xaml::Controls::ItemsControl const& control,
                                 Orientation orientation);

/// `elementItems = true` on a TreeView: its items are elements, shown as the elements they are.
/// With no ItemTemplate a TreeView writes the name of the type, and a DataTemplate is made from
/// XAML text only: a ContentControl that shows the content of its node.
void set_element_items(winrt::Microsoft::UI::Xaml::Controls::TreeView const& control, bool on);

// `leftItems` and `rightItems` having a mode. SwipeControl.LeftItems is a SwipeItems, a vector of
// SwipeItem with a Mode besides; wxl stands every such vector class for the collection itself, and
// a collection has no mode, so the mode is a property of the control and is set on its items.

void set_left_items_mode(winrt::Microsoft::UI::Xaml::Controls::SwipeControl const& control,
                         SwipeMode mode);

void set_right_items_mode(winrt::Microsoft::UI::Xaml::Controls::SwipeControl const& control,
                          SwipeMode mode);

/// `autoSuggestBox = ...` on a NavigationView: the search box of the pane, which the framework
/// lets be none. A wrapper cannot be empty, so the property takes a core::nullable and an empty
/// one clears the box.
void set_auto_suggest_box(winrt::Microsoft::UI::Xaml::Controls::NavigationView const& view,
                          core::nullable<AutoSuggestBox> const& box);

/// `fileType = u".txt"` on an open picker: one entry appended to the picker's own list.
void add_file_type(winrt::Microsoft::Windows::Storage::Pickers::FileOpenPicker const& picker,
                   hstring_param const& extension);

/// `fileTypeChoice = ...` on a save picker: one entry appended to the picker's own list.
void add_file_type_choice(winrt::Microsoft::Windows::Storage::Pickers::FileSavePicker const& picker,
                          FileTypeChoice const& choice);

}  // namespace wxl::impl
