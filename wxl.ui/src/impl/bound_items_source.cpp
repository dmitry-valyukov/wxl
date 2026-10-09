#include "bound_items_source.h"

namespace wxl::impl {

namespace {

using winrt::Windows::Foundation::Collections::CollectionChange;
using winrt::Windows::Foundation::Collections::IIterator;
using winrt::Windows::Foundation::Collections::IVectorChangedEventArgs;
using winrt::Windows::Foundation::Collections::IVectorView;

// What VectorChanged hands its handlers: which change, and where. One per notification,
// from the pool, so a handler that keeps it finds it as it was.
class vector_change final : public sta_com_object<vector_change, IVectorChangedEventArgs>
{
public:
    vector_change(CollectionChange change, uint32_t index) noexcept : change_(change), index_(index) {}

    int32_t __stdcall get_CollectionChange(int32_t* change) noexcept final {
        *change = static_cast<int32_t>(change_);
        return 0;
    }

    int32_t __stdcall get_Index(uint32_t* index) noexcept final {
        *index = index_;
        return 0;
    }

private:
    CollectionChange change_;
    uint32_t index_;
};

// The walk First() hands out. It reads the source as it is at each step, the way a view of
// it would, and keeps the source alive while it walks.
class slot_iterator final : public sta_com_object<slot_iterator, IIterator<item_object>>
{
public:
    explicit slot_iterator(bound_items_source& source) noexcept : source_(&source) { source.AddRef(); }

    ~slot_iterator() { source_->Release(); }

    int32_t __stdcall get_Current(void** item) noexcept final try {
        if (at_ >= source_->size()) {
            *item = nullptr;
            return com_out_of_bounds;
        }
        *item = source_->slot(at_).add_ref_abi();
        return 0;
    } catch (...) {
        *item = nullptr;
        return winrt::to_hresult();
    }

    int32_t __stdcall get_HasCurrent(bool* has) noexcept final {
        *has = at_ < source_->size();
        return 0;
    }

    int32_t __stdcall MoveNext(bool* has) noexcept final {
        if (at_ < source_->size()) ++at_;
        *has = at_ < source_->size();
        return 0;
    }

    int32_t __stdcall GetMany(uint32_t capacity, void** items, uint32_t* actual) noexcept final {
        uint32_t taken = 0;
        int32_t const result = source_->GetMany(at_, capacity, items, &taken);
        at_ += taken;
        *actual = taken;
        return result;
    }

private:
    bound_items_source* source_;  // one reference
    uint32_t at_ = 0;
};

}  // namespace

bound_items_source::bound_items_source(void const* followed, uint32_t count, bound_source::item_builder build)
    : list_(followed), build_(std::move(build)), slots_(count, nullptr) {}

bound_items_source::~bound_items_source() {
    for (bound_slot* const slot : slots_) {
        if (slot) drop(slot);
    }
    for (handler const& each : handlers_) {
        each.delegate->Release();
    }
}

void bound_items_source::disarm() noexcept {
    list_ = nullptr;
    build_ = std::nullopt;
}

Object bound_items_source::build(uint32_t at) const {
    int64_t const place = at < size() ? position(at) : -1;
    if (!build_ || place < 0) return Object::copy_from_abi(nullptr);
    return (*build_)(static_cast<uint32_t>(place));
}

int64_t bound_items_source::position(uint32_t at) const noexcept {
    if (lag_.count == 0 || at < lag_.at) return at;
    if (!lag_.erased) return int64_t{at} + lag_.count;
    if (at < lag_.at + lag_.count) return -1;
    return int64_t{at} - lag_.count;
}

bound_slot& bound_items_source::slot(uint32_t at) {
    assert(at < size());
    if (!slots_[at]) slots_[at] = new bound_slot{this, at};
    return *slots_[at];
}

void bound_items_source::renumber(uint32_t from) noexcept {
    for (uint32_t at = from; at < size(); ++at) {
        if (bound_slot* const slot = slots_[at]) slot->index_ = at;
    }
}

void bound_items_source::drop(bound_slot* slot) noexcept {
    slot->source_ = nullptr;
    slot->Release();
}

void bound_items_source::insert(uint32_t at, uint32_t still) {
    assert(at <= size());
    slots_.insert(slots_.begin() + at, nullptr);
    renumber(at + 1);
    lag_ = {at + 1, still, false};
    raise(CollectionChange::ItemInserted, at);
}

void bound_items_source::erase(uint32_t at, uint32_t still) {
    assert(at < size());
    bound_slot* const gone = slots_[at];
    slots_.erase(slots_.begin() + at);
    renumber(at);
    if (gone) drop(gone);
    lag_ = {at, still, true};
    raise(CollectionChange::ItemRemoved, at);
}

void bound_items_source::replace(uint32_t at) {
    assert(at < size());
    lag_ = {};
    if (bound_slot* const gone = std::exchange(slots_[at], nullptr)) drop(gone);
    raise(CollectionChange::ItemChanged, at);
}

void bound_items_source::reset(uint32_t count) {
    lag_ = {};
    core::sta_vector<bound_slot*> gone(count, nullptr);
    gone.swap(slots_);
    for (bound_slot* const slot : gone) {
        if (slot) drop(slot);
    }
    raise(CollectionChange::Reset, 0);
}

// Every handler is called with the source as it is after this one change. The handlers
// are copied first: one may remove itself, or another, while it is called.
void bound_items_source::raise(CollectionChange change, uint32_t index) noexcept {
    if (handlers_.empty()) return;
    try {
        com_ptr<vector_change> const args{new vector_change{change, index}};
        core::sta_vector<handler_abi*> called;
        called.reserve(handlers_.size());
        for (handler const& each : handlers_) {
            each.delegate->AddRef();
            called.push_back(each.delegate);
        }
        void* const sender = static_cast<interface_vtable<winrt::Windows::Foundation::Collections::IObservableVector<item_object>>*>(this);
        void* const what = args->identity();
        for (handler_abi* const delegate : called) {
            static_cast<void>(delegate->Invoke(sender, what));
            delegate->Release();
        }
    } catch (...) {
        // Out of memory for the arguments: the control is not told, as it would not be told
        // by a vector that could not raise its event either.
    }
}

int32_t __stdcall bound_items_source::GetAt(uint32_t index, void** item) noexcept try {
    if (index >= size()) {
        *item = nullptr;
        return com_out_of_bounds;
    }
    *item = slot(index).add_ref_abi();
    return 0;
} catch (...) {
    *item = nullptr;
    return winrt::to_hresult();
}

int32_t __stdcall bound_items_source::get_Size(uint32_t* count) noexcept {
    *count = size();
    return 0;
}

// The vector is its own view: it is read-only to everyone already, and a view that
// follows its source is what a control expects of one.
int32_t __stdcall bound_items_source::GetView(void** view) noexcept {
    AddRef();
    *view = static_cast<interface_vtable<IVectorView<item_object>>*>(this);
    return 0;
}

// A slot knows its place, so finding one is asking it -- no walk through the items.
int32_t __stdcall bound_items_source::IndexOf(void* item, uint32_t* index, bool* found) noexcept {
    *index = 0;
    *found = false;
    if (auto const slot = own_object<bound_slot>(item); slot && slot->source_ == this) {
        *index = slot->index_;
        *found = true;
    }
    return 0;
}

int32_t __stdcall bound_items_source::GetMany(uint32_t start, uint32_t capacity, void** items,
                                              uint32_t* actual) noexcept try {
    uint32_t taken = 0;
    for (; taken < capacity && start + taken < size(); ++taken) {
        items[taken] = slot(start + taken).add_ref_abi();
    }
    *actual = taken;
    return 0;
} catch (...) {
    *actual = 0;
    return winrt::to_hresult();
}

int32_t __stdcall bound_items_source::First(void** iterator) noexcept try {
    auto* const walk = new slot_iterator{*this};
    *iterator = static_cast<interface_vtable<IIterator<item_object>>*>(walk);
    return 0;
} catch (...) {
    *iterator = nullptr;
    return winrt::to_hresult();
}

int32_t __stdcall bound_items_source::add_VectorChanged(void* delegate, winrt::event_token* token) noexcept try {
    auto* const added = static_cast<handler_abi*>(delegate);
    handlers_.push_back({++last_token_, added});
    added->AddRef();
    token->value = last_token_;
    return 0;
} catch (...) {
    return winrt::to_hresult();
}

int32_t __stdcall bound_items_source::remove_VectorChanged(winrt::event_token token) noexcept {
    auto const found = std::ranges::find(handlers_, token.value, &handler::token);
    if (found != handlers_.end()) {
        handler_abi* const removed = found->delegate;
        handlers_.erase(found);
        removed->Release();
    }
    return 0;
}

winrt::Windows::Foundation::IInspectable bound_element(winrt::Windows::Foundation::IInspectable const& item) {
    auto const slot = own_object<bound_slot>(winrt::get_abi(item));
    if (!slot || !slot->source()) return nullptr;
    Object const element = slot->source()->build(slot->index());
    winrt::Windows::Foundation::IInspectable made;
    if (element) winrt::copy_from_abi(made, static_cast<void*>(element.get_abi()));
    return made;
}

// ---- bound_source: the side an application's header sees ----

bound_source::bound_source(void const* list, uint32_t size, item_builder build)
    : source_(new bound_items_source{list, size, std::move(build)}) {}

bound_source::bound_source(bound_source&& other) noexcept : source_(std::exchange(other.source_, nullptr)) {}

bound_source::~bound_source() {
    if (source_) {
        source_->disarm();
        source_->Release();
    }
}

void bound_source::operator()(core::list_change const& change) const noexcept {
    source_->apply(change);
}

int64_t bound_position(void const* list, Object const& box) noexcept {
    if (!list || !box) return -1;
    auto const slot = own_object<bound_slot>(static_cast<void*>(box.get_abi()));
    if (!slot || !slot->source() || slot->source()->list() != list) return -1;
    return slot->source()->position(slot->index());
}

}  // namespace wxl::impl
