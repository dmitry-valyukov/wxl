#pragma once

// Boxed values and their vectors: a number or a string as an object, and the way back, and a vector
// of boxed values as the object a control's ItemsSource takes -- what an item of ItemsSource or of
// ItemsControl.Items is when the application keeps its data in a vector of its own and hands the
// control the position of each (see ItemTemplate.h, IndexList), or when the item is just text; and
// what a content that is just text is. The words are cppwinrt's: winrt::box_value and
// winrt::unbox_value.

#include <cstdint>
#include <span>
#include <string>

#include "Object.h"
#include "hstring_param.h"

namespace wxl {

/// The number as an object: `list.items().append(boxValue(index))`.
Object boxValue(int64_t value);

/// The text as an object: `list.items().append(boxValue(u"text"))`.
Object boxValue(hstring_param const& text);

/// What a box made by boxValue holds, read back as `T`: int64_t or hstring. An object that is
/// anything else reads as the type's nothing, as winrt::unbox_value_or answers -- -1 for a
/// number, an empty string for text.
template <class T>
T unboxValue(Object const& boxed);

/// The number in the box; -1 for an object that is anything else.
template <>
int64_t unboxValue<int64_t>(Object const& boxed);

/// The text of a boxed string, as the very HSTRING the box holds -- one reference more and no
/// copy; empty for an object that is anything else.
template <>
hstring unboxValue<hstring>(Object const& boxed);

/// The strings as an object: a vector of boxed strings, a fresh one on every call, so a control
/// given it keeps a list nobody changes under it.
Object stringList(std::span<std::u16string const> items);

/// Any objects as the object an ItemsSource takes: a vector of them, a fresh one on every call
/// (TreeViewItem with its children below it, a list of elements).
Object objectList(std::span<Object const> items);

}  // namespace wxl
