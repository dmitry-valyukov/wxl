#pragma once

// Walking a WinRT list -- a VectorView, a Collection -- one index at a time.
//
// Each step asks the object behind the list for the element at that index,
// the way cppwinrt's own fast iterator over a vector does: one GetAt a step,
// where an IIterator would be an object of its own and two calls a step
// (Current, MoveNext). Nothing is read ahead and nothing is kept, so what the
// walk sees is what the list holds at the moment each element is asked for.
//
// The iterator holds the list by pointer and is good for as long as the
// list is: a range-for over a temporary keeps that temporary alive for the
// whole loop.

#include "../core.h"

namespace wxl::impl {

template <typename List>
class index_iterator {
public:
    // An element is made on demand -- a wrapper built, a string referenced --
    // so it is handed out by value and there is nothing to point into.
    using value_type = decltype(std::declval<List const&>().getAt(0));
    using reference = value_type;
    using pointer = void;
    using difference_type = std::ptrdiff_t;
    using iterator_concept = std::forward_iterator_tag;
    using iterator_category = std::input_iterator_tag;

    index_iterator() = default;
    index_iterator(List const* list, uint32_t index) noexcept : list_(list), index_(index) {}

    value_type operator*() const { return list_->getAt(index_); }

    index_iterator& operator++() noexcept {
        ++index_;
        return *this;
    }

    index_iterator operator++(int) noexcept {
        auto const before = *this;
        ++index_;
        return before;
    }

    bool operator==(index_iterator const& other) const noexcept { return index_ == other.index_; }

private:
    List const* list_ = nullptr;
    uint32_t index_ = 0;
};

}  // namespace wxl::impl
