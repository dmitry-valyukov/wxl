#pragma once

// impl::list_mirror -- one change of an observable_list, done to a view of it.
//
// A list says what happened in one change (core::list_change): count items inserted or
// erased at a place, one replaced, everything reset. A view that is a WinRT vector says it
// an item at a time -- IObservableVector has ItemInserted, ItemRemoved, ItemChanged and
// Reset and nothing for a run -- so the mirror takes a run apart into its items, in the
// order a vector edited by hand would raise them: inserted at, at + 1, ...; removed at
// `at` as many times as there were items. A view sees every state in between, each one
// item away from the last, which is what a control listening to a vector expects.
//
// Many items at once are cheaper as one reset than as a call per item, so above a
// threshold a run becomes a reset to the new size.
//
// The list is past the whole run before the mirror is called, so inside a run the view is
// some items behind it, and each step says how many: what the view reads from the list
// between two steps must go round the items still to come.
//
// The mirror is a template over what it writes to, so that the same code drives the source
// a control is given (impl::bound_items_source) and a plain vector in a test. The list has
// already changed when the mirror is called: a sink reading an item reads its new value.

#include "../core.h"

namespace wxl::impl {

/// Above this many items, a run inserted or erased goes to the view as one reset. A guess
/// to be refined by measurement in the probe: ListView and ItemsRepeater re-realise only
/// what is on screen after a reset, while each item of a run costs a notification and the
/// bookkeeping of every position after it.
inline constexpr uint32_t list_reset_threshold = 64;

/// What a view does for the mirror. Positions are the view's own, as they stand when the
/// call is made:
///  - `insert(at, still)`: the item at `at` is new; what stood there and after moves up by
///    one. `still` more items of the same run follow it in the list, and the view is told
///    of them next: until then, its items from `at + 1` stand `still` places later in the
///    list;
///  - `erase(at, still)`: the item at `at` is gone; what followed moves down by one. `still`
///    more items of the same run are gone from the list already, the view's items from
///    `at` on: until the next call they lead to nothing, and the items after them stand
///    `still` places earlier in the list;
///  - `replace(at)`: the item at `at` is another one;
///  - `reset(size)`: everything is new, `size` items of it;
///  - `size()`: how many items the view has.
template <class Sink>
concept list_sink = requires(Sink& sink, uint32_t at) {
    sink.insert(at, at);
    sink.erase(at, at);
    sink.replace(at);
    sink.reset(at);
    { sink.size() } -> std::convertible_to<uint32_t>;
};

/// Does each change of a list to `sink`. Holds the sink by reference: the watch that owns
/// the mirror owns, or outlives, what it writes to.
template <list_sink Sink>
class list_mirror
{
public:
    explicit list_mirror(Sink& sink, uint32_t threshold = list_reset_threshold) noexcept
        : sink_(&sink), threshold_(threshold) {}

    void operator()(core::list_change const& change) const {
        switch (change.kind) {
        case core::list_change::inserted:
            if (change.count > threshold_) {
                sink_->reset(static_cast<uint32_t>(sink_->size()) + change.count);
            } else {
                for (uint32_t k = 0; k != change.count; ++k) sink_->insert(change.at + k, change.count - 1 - k);
            }
            break;
        case core::list_change::erased:
            assert(change.count <= sink_->size() && "wxl: the view of a list holds fewer items than were erased");
            if (change.count > threshold_) {
                sink_->reset(static_cast<uint32_t>(sink_->size()) - change.count);
            } else {
                for (uint32_t k = 0; k != change.count; ++k) sink_->erase(change.at, change.count - 1 - k);
            }
            break;
        case core::list_change::replaced:
            sink_->replace(change.at);
            break;
        case core::list_change::reset:
            sink_->reset(change.count);
            break;
        }
    }

private:
    Sink* sink_;
    uint32_t threshold_;
};

}  // namespace wxl::impl
