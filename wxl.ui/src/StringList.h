#pragma once

// A list of strings as the object a control's ItemsSource takes, and the way
// back from one item the control hands out.
//
// ItemsSource wants a collection of objects, and a string in it is a boxed
// one; wxl has no box of its own to offer for that, so the pair lives here:
// `stringList` makes the vector of boxed strings, `stringOf` unboxes what an
// event brings back -- the chosen suggestion, say.

#include <span>
#include <string>

#include "Object.h"
#include "hstring_param.h"

namespace wxl {

/// The strings as an object: a vector of boxed strings, a fresh one on every
/// call, so a control given it keeps a list nobody changes under it.
Object stringList(std::span<std::u16string const> items);

/// Any objects as the object an ItemsSource takes: a vector of them, a fresh one on every call
/// (TreeViewItem with its children below it, a list of elements).
Object objectList(std::span<Object const> items);

/// One string as an object -- what an item of ItemsControl.Items is when it is just text:
/// `list.items().append(stringBox(u"text"))`.
Object stringBox(hstring_param const& text);

/// The text of a boxed string, as the very HSTRING the box holds -- one
/// reference more and no copy; empty for an object that is anything else.
hstring stringOf(Object const& boxed);

}  // namespace wxl
