#pragma once

// An integer as an object, and the way back -- what an item of ItemsSource is when the application
// keeps its data in a vector of its own and hands the control the position of each (see ItemBuilder).
// The same pair for strings is stringBox / stringOf in StringList.h.

#include <cstdint>

#include "Object.h"

namespace wxl {

/// The number as an object: `list.items().append(intBox(index))`.
Object intBox(int64_t value);

/// The numbers 0 ... count - 1 as the object an ItemsSource takes: one item per position of the
/// application's own vector, which the item template reads back with intOf.
Object indexList(int64_t count);

/// The number in a box made by intBox; -1 for an object that is anything else.
int64_t intOf(Object const& boxed);

}  // namespace wxl
