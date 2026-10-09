module;

#include "abi.h"

export module wxl.core:observable_list;

import :binding_scope;
import :event;
import :observable;
import :sta_stl;
import std;

export namespace wxl::core {

/// What happened to an observable_list, said exactly enough for a view to do the same to
/// itself: one operation of the view per change, nothing to compare or guess.
struct list_change {
    enum kind_t : uint8_t {
        /// `count` items now stand from `at` on; what stood there moved up.
        inserted,
        /// The `count` items that stood from `at` on are gone; what followed moved down.
        erased,
        /// The item at `at` is another one now; `count` is 1.
        replaced,
        /// Everything is new: `at` is 0 and `count` the new size.
        reset,
    };

    kind_t kind;
    uint32_t at;
    uint32_t count;

    friend bool operator==(list_change const&, list_change const&) = default;
};

template <class T>
class observable_list;

/**
 * A list seen read-only: `observable_list<T const>`. The model keeps an
 * `observable_list<T>` and hands this out where a view follows the items and
 * binds to them but only the model changes them -- as `observable<T const>`
 * is to a field:
 *
 *     struct Search {
 *         observable_list<Hit const>& hits() { return hits_; }
 *     private:
 *         observable_list<Hit> hits_;
 *     };
 *
 * Everything but changing is here: the items, the size as a field of its own,
 * the application's watches and the bindings' watches. The constructors and
 * the destructor are protected, so there is no read-only list without the list
 * behind it.
 */
template <class T>
class observable_list<T const>
{
public:
    // Spelled noexcept for the reason observable's is: a watcher is called from
    // inside fire(), which has nobody to hand an exception to.
    using changed_event = event<void(list_change const&) noexcept>;

    // A field, not a handle, for observable's reason: a copy would be a second
    // list no binding to the first would ever hear of.
    observable_list(observable_list const&) = delete;
    observable_list& operator=(observable_list const&) = delete;

    uint32_t size() const noexcept { return static_cast<uint32_t>(items_.size()); }

    bool empty() const noexcept { return items_.empty(); }

    T const& operator[](uint32_t at) const noexcept {
        assert(at < size());
        return items_[at];
    }

    T const* begin() const noexcept { return items_.data(); }

    T const* end() const noexcept { return items_.data() + items_.size(); }

    /// The items now -- what follow() hands a function for a list among its sources.
    std::span<T const> get() const noexcept { return {items_.data(), items_.size()}; }

    /// The size as a field, for what depends on it alone -- "nothing here yet", "found:
    /// N" -- and is bound like any other field. It changes after the list's bindings
    /// have heard of the change and before its application watches do.
    observable<uint32_t const>& count() noexcept { return count_; }

    /// Watches for changes; the returned cookie removes the watch. Called after the
    /// bindings, with the change already made.
    template <class F>
    cookie_t on_change(F&& callback) {
        return changed_.add(std::forward<F>(callback));
    }

    bool remove_change(cookie_t cookie) { return changed_.remove(cookie); }

    /// Watches for changes on behalf of a binding -- a view mirroring the list -- the list
    /// keeping the watch, as observable::watch_for_binding does, and a binding_scope open
    /// meanwhile collecting it too.
    template <class F>
    void watch_for_binding(F&& callback) {
        impl::add_binding_watch(bound_, std::forward<F>(callback));
    }

    /// Cuts every binding watch, leaving the application's alone; see observable::unbind.
    void unbind() { bound_.clear(); }

protected:
    observable_list() = default;
    explicit observable_list(sta_vector<T> items) : items_(std::move(items)), count_(size()) {}
    ~observable_list() = default;

    // The change is made already: the bindings hear first, so a view shows it before
    // anything else learns of it; then the size; then the application. The list is not
    // changed again from inside: a watcher that did would hand the ones after it the
    // second change before the first.
    void notify(list_change const& change) noexcept {
        assert(!notifying_ && "wxl: an observable_list is changed by one of its own watchers");
        notifying_ = true;
        bound_.fire(change);
        count_.set(size());
        changed_.fire(change);
        notifying_ = false;
    }

    sta_vector<T> items_;
    observable<uint32_t> count_;
    // After the items and the size, so that the watches go first: a binding's watch lets
    // go of its view while the list it mirrored is still whole.
    changed_event bound_;
    changed_event changed_;
    bool notifying_ = false;
};

namespace impl {

// The read-only list a list extends, named apart for the reason readonly_observable is.
template <class T>
using readonly_observable_list = observable_list<T const>;

}  // namespace impl

/**
 * A list that says exactly what changed in it -- the model side of a list view.
 *
 * A view showing a collection needs to hear more than "it changed": a shelf that grew by
 * one book should get one new row, not a rebuilt shelf, and a list of a thousand search
 * hits should not be compared item by item to find out that all of them are new. So
 * every change here is one of four, with its place -- inserted, erased, replaced, reset
 * (list_change) -- and a view does the same to itself in one operation.
 *
 * Like `observable`, it is a field: held by value in the model that owns it, neither
 * copied nor moved, its identity the model's. A view binds to the field, not to its
 * contents, so a model whose contents are replaced wholesale -- another book's table of
 * contents -- assigns them rather than making a new list. The items live in the STA pool,
 * and so does every watch.
 *
 * An item that changes in part while it is shown is best an item with fields of its own,
 * held by pointer -- `observable_list<intrusive_ptr<Card>>`, a card with an
 * `observable<u16_text> progress` -- so that its view binds to the field and nothing is
 * rebuilt. An item that is a plain value is changed by replace(). Equal values say
 * nothing: replace() and assign() of what is there already are silent, as observable's
 * set() is, when the items can be compared.
 */
template <class T>
class observable_list : public impl::readonly_observable_list<T>
{
    using readonly = impl::readonly_observable_list<T>;

public:
    observable_list() = default;
    explicit observable_list(sta_vector<T> items) : readonly(std::move(items)) {}

    void push_back(T item) { insert(this->size(), std::move(item)); }

    /// Puts `item` at `at`, moving what stood there and after it up by one.
    void insert(uint32_t at, T item) {
        assert(at <= this->size());
        this->items_.insert(this->items_.begin() + at, std::move(item));
        this->notify({list_change::inserted, at, 1});
    }

    /// Puts all of `items` at `at`, in their order: one change for all of them, and none
    /// for none.
    template <std::ranges::input_range R>
        requires std::convertible_to<std::ranges::range_reference_t<R>, T>
    void insert_range(uint32_t at, R&& items) {
        assert(at <= this->size());
        auto const before = this->items_.size();
        this->items_.insert_range(this->items_.begin() + at, std::forward<R>(items));
        if (auto const added = static_cast<uint32_t>(this->items_.size() - before)) {
            this->notify({list_change::inserted, at, added});
        }
    }

    /// Takes out the `count` items from `at` on.
    void erase(uint32_t at, uint32_t count = 1) {
        assert(at <= this->size() && count <= this->size() - at);
        if (count == 0) return;
        auto const first = this->items_.begin() + at;
        this->items_.erase(first, first + count);
        this->notify({list_change::erased, at, count});
    }

    /// Puts `item` in place of the one at `at`; says nothing if the two are equal.
    void replace(uint32_t at, T item) {
        assert(at < this->size());
        if constexpr (std::equality_comparable<T>) {
            if (this->items_[at] == item) return;
        }
        this->items_[at] = std::move(item);
        this->notify({list_change::replaced, at, 1});
    }

    /// Replaces everything at once -- one reset however much changed; says nothing if the
    /// new items equal the old.
    void assign(sta_vector<T> items) {
        if constexpr (std::equality_comparable<T>) {
            if (this->items_ == items) return;
        }
        this->items_ = std::move(items);
        this->notify({list_change::reset, 0, this->size()});
    }

    /// Empties the list: a reset to nothing, and silence if it was empty already.
    void clear() {
        if (this->items_.empty()) return;
        this->items_.clear();
        this->notify({list_change::reset, 0, 0});
    }

    /// Follows other fields: whenever any of the sources changes, `fn` -- the last
    /// argument -- is called with the value of each and what it returns is assigned, so
    /// it returns the items (an `sta_vector<T>`, or what converts to one). Computed once
    /// right here too, as observable::follow is. A source is an observable, whose value
    /// is its `get()`, or another list, whose value is its items as a span: a filtered
    /// view of a list follows that list.
    ///
    /// The watches live on the sources and point back at this list, so it has to be
    /// there whenever a source changes -- as it is when all of them are fields of one
    /// model. They are application watches, which unbind() leaves alone.
    template <class... Args>
        requires (sizeof...(Args) >= 2)
    observable_list& follow(Args&&... args) {
        auto wire = [this]<std::size_t... I>(std::index_sequence<I...>, auto sources_and_fn) {
            auto recompute = [this, fn = std::get<sizeof...(I)>(std::move(sources_and_fn)),
                              ...sources = &std::get<I>(sources_and_fn)]() noexcept {
                assign(fn(sources->get()...));
            };
            recompute();
            (std::get<I>(sources_and_fn).on_change([recompute](auto const&) noexcept { recompute(); }), ...);
        };
        wire(std::make_index_sequence<sizeof...(Args) - 1>{},
             std::forward_as_tuple(std::forward<Args>(args)...));
        return *this;
    }
};

}  // export namespace wxl::core
