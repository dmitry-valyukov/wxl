#pragma once

// impl::bound_items_source -- the ItemsSource a list bound with `itemsSource =
// BindOutput{list, build}` gives its control: a WinRT vector of wxl's own, whose items are
// slots.
//
// A control holds on to the items it is given -- the item a container shows, clickedItem,
// selectedItem -- and compares them by identity. A box holding a position in the list
// would point elsewhere after an insertion before it. A slot is the item itself as the
// control sees it: one small object per item, which knows its source and its place in it,
// and the place moves when items are inserted or erased before it -- an integer each, with
// nothing told to anyone. So a slot the control hands back always leads to the item it was
// made for, or, once that item is gone, to nothing.
//
// Slots are made when the control asks for them (GetAt, GetMany, an iterator): a list of a
// hundred thousand items has a slot for each item the control has seen, not for each item.
// A replaced item gets a new slot, so the control sees another object at that place and
// shows it anew.
//
// The vector is read-only to the control: the list is written by its model, and every
// method that would change it answers E_NOTIMPL. Changes reach it through list_mirror,
// the source being its sink: one item at a time, VectorChanged after each, or one Reset.
// Inside a run the list is already past it, so a slot is read through its place in the
// list (position), not its number among the slots: a control that builds an element or
// hands out its selection inside VectorChanged reaches the slot's own item.
//
// Only the Windows.Foundation projection is needed here, not WinUI: the source and its
// slots are tested on their own (test/bound_items_source_test.cpp).

#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Foundation.h>

#include "../Object.h"
#include "bound_list.h"
#include "list_mirror.h"
#include "sta_com.h"

namespace wxl::impl {

class bound_items_source;

/// One item of a bound source, as the control holds it.
class bound_slot final : public sta_com_object<bound_slot, winrt::Windows::Foundation::IInspectable>
{
public:
    static constexpr winrt::guid own_iid{0x08c8704b, 0x15b9, 0x4b35, {0xb5, 0xed, 0x83, 0xaf, 0xb1, 0x00, 0x6a, 0x65}};

    bound_slot(bound_items_source* owner, uint32_t at) noexcept : source_(owner), index_(at) {}

    /// The source the slot stands in, or null once its item is gone or the source is.
    bound_items_source* source() const noexcept { return source_; }

    /// Where its item stands in the list now.
    uint32_t index() const noexcept { return index_; }

private:
    friend class bound_items_source;

    bound_items_source* source_;
    uint32_t index_;
};

using item_object = winrt::Windows::Foundation::IInspectable;

class bound_items_source final
    : public sta_com_object<bound_items_source,
                            winrt::Windows::Foundation::Collections::IObservableVector<item_object>,
                            winrt::Windows::Foundation::Collections::IVector<item_object>,
                            winrt::Windows::Foundation::Collections::IVectorView<item_object>,
                            winrt::Windows::Foundation::Collections::IIterable<item_object>>
{
public:
    static constexpr winrt::guid own_iid{0xe7205a74, 0x9eb2, 0x42f0, {0xbb, 0x16, 0x3f, 0xea, 0x6d, 0xa6, 0x3b, 0x2b}};

    bound_items_source(void const* followed, uint32_t count, bound_source::item_builder build);
    ~bound_items_source();

    /// The list it follows, or null once disarmed.
    void const* list() const noexcept { return list_; }

    /// One change of the list, done to the slots and told to the control.
    void apply(core::list_change const& change) { list_mirror{*this}(change); }

    /// The list is gone, or no longer followed: the builder and the list are forgotten, and
    /// an item asked for from now on is built as nothing.
    void disarm() noexcept;

    /// The element of the item of the slot at `at`, built by the application's function; an
    /// empty object once disarmed or once its item is gone from the list.
    Object build(uint32_t at) const;

    /// Where the item of the slot at `at` stands in the list, or -1 if the list has it no
    /// more. Out of a run it is the slot's own number; inside one (list_mirror) the slots
    /// lag behind the list.
    int64_t position(uint32_t at) const noexcept;

    /// The slot of the item at `at`, made now if the control has not asked for it before.
    bound_slot& slot(uint32_t at);

    // The sink of list_mirror.
    uint32_t size() const noexcept { return static_cast<uint32_t>(slots_.size()); }
    void insert(uint32_t at, uint32_t still);
    void erase(uint32_t at, uint32_t still);
    void replace(uint32_t at);
    void reset(uint32_t count);

    // IVector<IInspectable> and IVectorView<IInspectable>: one definition each for both.
    int32_t __stdcall GetAt(uint32_t index, void** item) noexcept final;
    int32_t __stdcall get_Size(uint32_t* size) noexcept final;
    int32_t __stdcall GetView(void** view) noexcept final;
    int32_t __stdcall IndexOf(void* item, uint32_t* index, bool* found) noexcept final;
    int32_t __stdcall SetAt(uint32_t, void*) noexcept final { return com_not_implemented; }
    int32_t __stdcall InsertAt(uint32_t, void*) noexcept final { return com_not_implemented; }
    int32_t __stdcall RemoveAt(uint32_t) noexcept final { return com_not_implemented; }
    int32_t __stdcall Append(void*) noexcept final { return com_not_implemented; }
    int32_t __stdcall RemoveAtEnd() noexcept final { return com_not_implemented; }
    int32_t __stdcall Clear() noexcept final { return com_not_implemented; }
    int32_t __stdcall GetMany(uint32_t start, uint32_t capacity, void** items, uint32_t* actual) noexcept final;
    int32_t __stdcall ReplaceAll(uint32_t, void**) noexcept final { return com_not_implemented; }

    // IIterable<IInspectable>
    int32_t __stdcall First(void** iterator) noexcept final;

    // IObservableVector<IInspectable>
    int32_t __stdcall add_VectorChanged(void* delegate, winrt::event_token* token) noexcept final;
    int32_t __stdcall remove_VectorChanged(winrt::event_token token) noexcept final;

private:
    using handler_abi = winrt::impl::abi_t<winrt::Windows::Foundation::Collections::VectorChangedEventHandler<item_object>>;

    struct handler {
        int64_t token;
        handler_abi* delegate;  // one reference
    };

    // Each slot from `from` on is told where it stands now.
    void renumber(uint32_t from) noexcept;
    // A slot whose item is gone: it leads nowhere from now on, and the source lets go of it.
    static void drop(bound_slot* slot) noexcept;
    void raise(winrt::Windows::Foundation::Collections::CollectionChange change, uint32_t index) noexcept;

    // How the slots lag behind the list inside a run: `count` items of it are still to be
    // told, at the slot `at`. Inserted, they are in the list and not among the slots yet, so
    // the slots from `at` stand `count` places later in the list; erased, the slots
    // [at, at + count) are of items already gone, and those after stand `count` places
    // earlier. Out of a run, `count` is 0.
    struct lag_behind {
        uint32_t at = 0;
        uint32_t count = 0;
        bool erased = false;
    };

    void const* list_;
    core::nullable<bound_source::item_builder> build_;
    lag_behind lag_;
    // One per item; null where the control has not asked for the item yet. Each holds one
    // reference.
    core::sta_vector<bound_slot*> slots_;
    core::sta_vector<handler> handlers_;
    int64_t last_token_ = 0;
};

/// The element of an item a control asks for, when the item is a slot: built by its
/// source, inside whatever binding_scope is open. Null for anything else, and for a slot
/// whose item is gone or whose source is disarmed.
winrt::Windows::Foundation::IInspectable bound_element(winrt::Windows::Foundation::IInspectable const& item);

}  // namespace wxl::impl
