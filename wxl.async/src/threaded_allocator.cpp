module;
#include "pch.h"

module wxl.async;
import wxl.core;
import std;

namespace wxl::async {

namespace {

void* invalid_alloc(threaded_allocator*) noexcept {
    core::abort("threaded_allocator: invalid allocation size (0)");
}

}  // namespace

void threaded_allocator::init(uint32_t page_size) {
    if (s_current) [[unlikely]]
        core::abort("threaded_allocator::init: already initialized on this thread");

    threaded_allocator* alloc = new threaded_allocator(page_size);
    s_current = alloc;
    thread_group::at_thread_exit(&threaded_allocator::thread_cleanup, alloc);
}

void threaded_allocator::thread_cleanup(void* arg) noexcept {
    threaded_allocator* alloc = static_cast<threaded_allocator*>(arg);
    s_current = nullptr;
    alloc->release_ref();
}

threaded_allocator::threaded_allocator(uint32_t page_size) noexcept
    : page_size_(page_size < 4096 ? 4096 : (page_size + 4095) & ~4095u) {
    pages_.reserve(16);
}

threaded_allocator::~threaded_allocator() {
    // Discard whatever is still sitting in each size class's deferred inbox: it can only be
    // non-empty here if the very last free() of this allocator's lifetime went through the
    // cross-thread path and tipped the refcount to zero before anyone drained it. pop_all()
    // just empties each drain_stack's internal state (its own destructor asserts it is
    // empty); the memory itself belongs to a page released wholesale right below, so the
    // popped lists themselves need no further attention.
    for (auto& class_deferred : deferred_) class_deferred.pop_all();

    for (void* page : pages_) ::VirtualFree(page, 0, MEM_RELEASE);
}

void threaded_allocator::alloc_page() {
    std::byte* page = static_cast<std::byte*>(
        ::VirtualAlloc(nullptr, page_size_, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (!page) [[unlikely]]
        core::abort("threaded_allocator: page allocation failed");

    pages_.push_back(page);
    cursor_ = page;
    end_ = page + page_size_;
}

template <uint32_t N>
void* threaded_allocator::bump_alloc(threaded_allocator* self) noexcept {
    // Both compile-time constants: no per-call work to determine either the class or its
    // total physical size, unlike a runtime "user_size -> total_block_size" computation would
    // need.
    constexpr uint32_t idx = pool_index(1u << N);
    constexpr uint32_t total = (1u << N) + sizeof(void*);

    if (void* node = self->pop_free_list(idx)) [[likely]]
        return node;

    if (std::byte* c = self->cursor_, *c1 = c + total; c1 <= self->end_) [[likely]] {
        self->cursor_ = c1;
        return c;
    }

    // Current page is exhausted: reclaim this size class's own deferred inbox before paying
    // for a brand new page. deferred_[idx] is already linked exactly like a free list (same
    // first-word `next` layout), so the whole batch can be adopted in one splice -- no
    // per-node walk needed.
    self->free_lists_[idx] = self->deferred_[idx].pop_all();

    if (void* node = self->pop_free_list(idx)) return node;

    self->alloc_page();
    std::byte* c = self->cursor_;
    self->cursor_ = c + total;
    return c;
}

// Indexed by pool_index(size); entry 0 rejects size==0, entries 1..16 are unreachable (no
// size in [1, MaxBlockSize] maps there), entries 17..29 are bump_alloc<15> down to
// bump_alloc<3>. Entries 30..32 (classes N=2,1,0) collapse onto bump_alloc<3> too, since their total
// block size would otherwise not be a multiple of sizeof(void*) -- see the comment on
// s_alloc_table's declaration.
const threaded_allocator::alloc_fn threaded_allocator::s_alloc_table[33] = {
    invalid_alloc,   nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         &bump_alloc<15>, &bump_alloc<14>, &bump_alloc<13>,
    &bump_alloc<12>, &bump_alloc<11>, &bump_alloc<10>, &bump_alloc<9>,
    &bump_alloc<8>,  &bump_alloc<7>,  &bump_alloc<6>,  &bump_alloc<5>,
    &bump_alloc<4>,  &bump_alloc<3>,  &bump_alloc<3>,  &bump_alloc<3>,
    &bump_alloc<3>,
};

void threaded_allocator::free_foreign(uint32_t idx, void* block) {
    deferred_mem_block* node = ::new (block) deferred_mem_block();
    deferred_[idx].push(node);

    release_ref();
}

}  // namespace wxl::async
