#pragma once

// wxl::IndexList -- the positions in a vector of the application's own, as the object an
// ItemsSource takes: each item is a boxed number (boxValue), which the item template reads back
// with unboxValue<int64_t> and turns into the element for the application's own entry there.
//
// A wrapper over the platform's vector of objects, with no state of its own: what it asks of the
// vector it asks through the object it wraps, so a list a control hands back -- its ItemsSource
// -- is the same list again.

#include <cstdint>
#include <span>

#include "Object.h"

namespace wxl {

class IndexList : public Object
{
public:
    /// The numbers 0 ... count - 1: one item per position of the application's own vector.
    explicit IndexList(int64_t count);

    /// The positions the application picks: the items of a filtered or sorted view of its own
    /// vector.
    explicit IndexList(std::span<int64_t const> positions);

    /// The list a control was given, as it hands it back -- its ItemsSource. An object that is
    /// not a vector of objects stops the process: it was never an IndexList.
    explicit IndexList(Object const& list);

    /// An empty list that the control follows as it changes: what is appended or removed shows
    /// at once.
    static IndexList observable();

    void append(int64_t value) const;
    void insertAt(uint32_t index, int64_t value) const;
    void removeAt(uint32_t index) const;

    /// The number at a place.
    int64_t getAt(uint32_t index) const;

    /// The boxed object itself at a place: what a control compares by identity when it is told
    /// which item is selected.
    Object objectAt(uint32_t index) const;

    uint32_t size() const;
};

}  // namespace wxl
