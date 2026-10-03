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

/// The same for the positions the application picks: the items of a filtered or sorted view of its own vector.
Object indexList(int64_t const* positions, size_t count);

/// The number in a box made by intBox; -1 for an object that is anything else.
int64_t intOf(Object const& boxed);

/// An empty list of positions that the control follows as it changes: what is appended or removed
/// shows at once. Appended and removed with the functions below, read by the control as indexList is.
Object observableIndexList();

void indexListAppend(Object const& list, int64_t value);
void indexListRemoveAt(Object const& list, uint32_t index);
uint32_t indexListSize(Object const& list);

}  // namespace wxl
