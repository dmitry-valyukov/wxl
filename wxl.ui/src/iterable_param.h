#pragma once

// What a WinRT IIterable<T> parameter takes: a sequence handed to a call.
//
// A list wxl already holds -- a VectorView, a Collection -- is handed over as
// the WinRT object it is: both implement IIterable<T>, so the call gets that
// very object, and nothing is copied. Elements in memory -- a std::vector, an
// array, a braced list -- have no WinRT object behind them, and the call needs
// one, so they are copied into a WinRT vector of the call's own. That copy is
// honest: what the call takes is a sequence given to it, not a view of
// somebody else's data.
//
// Like hstring_param, it only borrows, and only for the duration of the call:
// it holds the wrapper or the elements by address, so it is a parameter and
// nothing else.
//
// T is what VectorView<T> holds: a wrapper, a number, or a type that crosses
// through impl/conversions.h.

#include "Collection.h"
#include "VectorView.h"

namespace wxl {

template <typename T>
class iterable_param {
public:
    iterable_param(VectorView<T> const& view) noexcept : list_(&view) {}
    iterable_param(Collection<T> const& collection) noexcept : list_(&collection) {}

    // Anything that keeps its elements side by side: a std::vector, a
    // std::array, a C array, a span.
    template <std::ranges::contiguous_range Range>
        requires std::ranges::sized_range<Range> &&
                 std::same_as<std::remove_cv_t<std::ranges::range_value_t<Range>>, T>
    iterable_param(Range const& items) noexcept
        : items_(std::ranges::data(items), std::ranges::size(items)) {}

    // `setStorageItems({first, second})`: the braced list lives until the end
    // of the call it is written in, which is as long as it is needed.
    iterable_param(std::initializer_list<T> items) noexcept : items_(items.begin(), items.size()) {}

    iterable_param(iterable_param const&) = delete;
    iterable_param& operator=(iterable_param const&) = delete;

    // The list to hand over as it is, or nothing when the elements are to be
    // copied from items().
    Object const* list() const noexcept { return list_; }
    std::span<T const> items() const noexcept { return items_; }

private:
    Object const* list_ = nullptr;
    std::span<T const> items_;
};

}  // namespace wxl
