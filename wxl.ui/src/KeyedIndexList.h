#pragma once

// wxl::keyedIndexList -- a list of positions that a repeater can keep its place in when the list is replaced.
//
// Like indexList, the items are positions in a vector of the application's own, boxed. What this adds is what
// MyItemsSource of the WinUI Gallery adds to a list of recipes: every item has a key (the position it holds,
// as text) and the list can say which index a key has now (IKeyIndexMapping), and it tells that it was replaced
// as a whole (CollectionChanged with Reset). A repeater that has the keys finds the item it was showing at the
// top again after a filter or a new order, and an ItemsRepeater layout is told that all its items changed.

#include <cstddef>
#include <cstdint>

#include "Object.h"

namespace wxl {

/// An empty keyed list.
Object keyedIndexList();

/// Replaces what the list holds with these positions and tells the control that it was replaced.
void keyedIndexListReset(Object const& list, int64_t const* positions, size_t count);

}  // namespace wxl
