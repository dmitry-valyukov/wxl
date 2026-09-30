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

namespace wxl {

/// The strings as an object: a vector of boxed strings, a fresh one on every
/// call, so a control given it keeps a list nobody changes under it.
Object stringList(std::span<std::u16string const> items);

/// The text of a boxed string; empty for an object that is anything else.
std::u16string stringOf(Object const& boxed);

}  // namespace wxl
