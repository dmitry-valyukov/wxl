module;

#include "abi.h"

export module wxl.async:threaded_allocator;

import :drain_stack;
import wxl.core;
import std;

export namespace wxl::async {

/// Node used when a block is handed to a foreign thread's per-size-class deferred-free stack.
/// Placement-constructed directly over the (already logically freed) block's own memory,
/// reusing the block's first machine word as both the intrusive-stack `next_` link and,
/// later, the free-list `next` link -- the two layouts are identical, so a whole reclaimed
/// batch can be spliced onto the free list as-is, with no per-node work.
struct deferred_mem_block : core::intrusive_slist_node<deferred_mem_block> {
};

/// Per-thread bump-page allocator with size-class free lists (same principle as
/// sta_memory_pool), except every instance belongs to exactly one thread instead of being a
/// process-wide singleton.
///
/// A threaded_allocator is created explicitly on the thread that will own it (init()), which
/// binds the instance to that thread's TLS slot and registers it with thread_group's
/// per-thread cleanup stack. Every allocated block is prefixed (at a negative offset from the
/// pointer handed to the caller) with a pointer back to its owning allocator, so free() can
/// tell, from the pointer alone, whether the freeing thread is the owning thread:
///  - same thread: the block goes straight back onto the local size-class free list.
///  - different thread: the block is handed to the owner's deferred_[idx] drain_stack --
///    the same size class it was allocated from, so reclaiming it later needs no per-node
///    routing. The owning thread reclaims the whole batch for a size class in one splice
///    (pop_all()) the next time that class's free list is empty and its page has run out.
///
/// The allocator is refcounted_mt-based: every allocated block holds an implicit reference,
/// taken in alloc() and released by whichever call (same-thread or cross-thread) frees it.
/// init() itself holds the first reference on behalf of "the owning thread is still alive",
/// released from thread_group's cleanup callback when that thread exits. The allocator is
/// therefore only destroyed once its owning thread has exited *and* every block it ever
/// handed out has been freed by someone -- whichever of those two events happens last.
class threaded_allocator : public core::refcounted_mt
{
public:
    inline static constexpr uint32_t MaxBlockSize = 1u << 15;  // 32k, matches sta_memory_pool
    inline static constexpr uint32_t DefaultPageSize = 1024 * 1024;  // 1M

    /// Creates and binds a new threaded_allocator to the calling thread. May be called only
    /// once per thread (aborts otherwise), and only from a thread thread_group can register
    /// cleanup for (a thread_group-spawned thread, or the main thread) -- see
    /// thread_group::at_thread_exit.
    ///
    /// page_size is rounded up to a 4096-byte (VirtualAlloc granularity) multiple; deliberately
    /// has no large enforced floor (unlike sta_memory_pool) so callers can pick a small page to
    /// exercise the cross-thread reclaim path deterministically, e.g. in tests/benchmarks.
    static void init(uint32_t page_size = DefaultPageSize);

    /// \return The allocator owning the calling thread, or nullptr if init() was never called
    ///         on this thread.
    static threaded_allocator* current() noexcept { return s_current; }

    /// size is classified by pool_index(size) alone (no header adjustment): each class's own
    /// bump_alloc<N>() already knows its class's total block size (user capacity plus the header)
    /// as a compile-time constant, exactly as sta_memory_pool's bump_alloc<N>() knows its own
    /// size -- so, unlike free(), alloc() never needs to compute anything about block layout
    /// at runtime, only look up which per-class function to call.
    static constexpr uint32_t pool_index(uint32_t size) noexcept {
        return std::countl_zero(size - 1);
    }

    /// Allocates size bytes. May only be called on the owning thread (the one that called
    /// init()); asserts otherwise.
    ///
    /// Defined here (not in the .cpp), like sta_memory_pool::alloc()/alloc_impl(), so that a
    /// caller such as operator new -- itself typically inline -- compiles down to exactly the
    /// size check, the pool_index() table lookup, and the one indirect call through
    /// s_alloc_table; there is no separate non-inlined threaded_allocator::alloc() frame for
    /// every allocation to pay for on top of that.
    static void* alloc(size_t size) {
        threaded_allocator* self = current();
        assert(self && "threaded_allocator::alloc: no allocator initialized on this thread");
        assert(size >= 1 && size <= MaxBlockSize &&
               "threaded_allocator: invalid allocation size");

        void* block = s_alloc_table[pool_index(static_cast<uint32_t>(size))](self);
        *static_cast<threaded_allocator**>(block) = self;
        self->add_ref();

        return static_cast<std::byte*>(block) + sizeof(void*);
    }

    /// Frees a block returned by alloc(). May be called from any thread; size must match the
    /// value originally passed to alloc() for this block. Inline for the same reason as
    /// alloc() -- unlike alloc(), free() never needs table dispatch at all: idx alone (no
    /// per-class total) is enough to push onto the right free list or deferred stack.
    ///
    /// idx is clamped to SmallestClassIndex to mirror s_alloc_table's own collapsing of
    /// classes N=2/1/0 onto bump_alloc<3>: a block alloc() handed out for e.g. a 2-byte request
    /// physically came from (and must go back onto) class 3's free list, not a nonexistent
    /// class-1 one.
    static void free(void* mem, size_t size) {
        std::byte* block = static_cast<std::byte*>(mem) - sizeof(void*);
        threaded_allocator* owner = *reinterpret_cast<threaded_allocator**>(block);
        const uint32_t idx = std::min(pool_index(static_cast<uint32_t>(size)), SmallestClassIndex);

        if (owner == current()) {
            owner->push_free_list(idx, block);
            owner->release_ref();
        } else {
            owner->free_foreign(idx, block);
        }
    }

    /// \return The number of pages ever committed via VirtualAlloc for this allocator.
    ///         Diagnostic only (e.g. for benchmarks confirming how much the deferred-reclaim
    ///         path is actually cutting down on fresh page allocation).
    size_t page_count() const noexcept { return pages_.size(); }

private:
    explicit threaded_allocator(uint32_t page_size) noexcept;
    ~threaded_allocator() override;

    // The allocator owning the calling thread, if init() has been called on it. An inline
    // static data member (rather than a thread_local hidden in the .cpp) so current() can be
    // defined -- and inlined -- right here in the header alongside alloc()/free().
    static inline thread_local threaded_allocator* s_current = nullptr;

    static void thread_cleanup(void* arg) noexcept;

    void alloc_page();
    void free_foreign(uint32_t idx, void* block);

    /// Per-size-class allocation, dispatched to via s_alloc_table[pool_index(size)]: checks
    /// the local free list, then the bump cursor, then reclaims deferred_[idx] as a whole,
    /// then finally commits a new page -- same shape as sta_memory_pool's bump_alloc<N>(), with
    /// `self` standing in for the state sta_memory_pool keeps in static/global storage.
    template <uint32_t N>
    static void* bump_alloc(threaded_allocator* self) noexcept;

    using alloc_fn = void* (*)(threaded_allocator*) noexcept;

    // Indexed by pool_index(size); entry 0 rejects the invalid size==0 request, entries
    // 1..16 are unreachable (no size in [1, MaxBlockSize] maps there), entries 17..29
    // are bump_alloc<15> down to bump_alloc<3>. Entries 30..32 (classes N=2,1,0, i.e. total block
    // sizes 12/10/9 bytes) collapse onto bump_alloc<3> (16 bytes) too, instead of getting their
    // own bump_alloc<2>/<1>/<0>, exactly as sta_memory_pool collapses its own bottom classes onto
    // bump_alloc<3> on 64-bit. The reason differs (sta_memory_pool's blocks below 8 bytes can't
    // even hold a free-list pointer; threaded_allocator's always can, in the header word),
    // but the constraint that forces the same collapse still applies: total_block_size =
    // (1<<N) + sizeof(void*) must itself be a multiple of sizeof(void*) for cursor_ to stay
    // pointer-aligned across allocations, and that only holds for N == 0/1/2 once they are
    // folded into N == 3. free() has to replicate this collapse by hand (see
    // SmallestClassIndex below) since it bypasses s_alloc_table entirely.
    static const alloc_fn s_alloc_table[33];

    // The lowest surviving class index -- pool_index(size) never needs to resolve any lower
    // than this, since classes below it are folded into it (see s_alloc_table above).
    // Spelled out via countl_zero directly rather than pool_index(1u << 3): MSVC does not
    // accept a call to a constexpr member function of this same class, still being defined,
    // from another static data member's initializer.
    static constexpr uint32_t SmallestClassIndex = std::countl_zero((1u << 3) - 1u);

    void* pop_free_list(uint32_t idx) noexcept {
        void* node = free_lists_[idx];
        if (node) [[likely]]
            free_lists_[idx] = *static_cast<void**>(node);
        return node;
    }

    void push_free_list(uint32_t idx, void* node) noexcept {
        *static_cast<void**>(node) = free_lists_[idx];
        free_lists_[idx] = node;
    }

    // Indexed by pool_index(size); same range as s_alloc_table / sta_memory_pool's dispatch tables.
    static constexpr size_t NumClasses = 33;

    void* free_lists_[NumClasses]{};
    std::byte* cursor_ = nullptr;
    std::byte* end_ = nullptr;
    uint32_t page_size_;
    std::vector<void*> pages_;

    /// Per-size-class cross-thread inbox for blocks freed by threads other than the owner;
    /// deferred_[idx] is drained into free_lists_[idx] in one splice whenever that class's
    /// free list is empty and its page has run out, before falling back to a new page.
    drain_stack<deferred_mem_block> deferred_[NumClasses];
};

}  // export namespace wxl::async
