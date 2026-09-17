module;

#include "abi.h"

export module wxl.core:sta_allocator;

import :allocator;
import :checks;
import :thread_heap;
import std;

export namespace wxl::core {

/// The allocator of the one STA thread: many small, short-lived blocks, with no locks and no
/// atomics, because no other thread may touch it. Every entry point asserts that in debug builds.
///
/// The only instance is a static built in the `lib` init segment, so the pool stands before any
/// object of the program and outlives all of them.
///
/// Up to MaxBlockSize a request is served from free lists carved out of pages. Above that it goes
/// to a private heap, and above half a page to std::malloc: a block that big never shares a page,
/// so the pool has nothing to gain from it.
///
/// Two things on the entry points look like noise and are not. A body defined in the class of a
/// named module is not inline unless it says so, and MSVC then keeps it out of the importers.
/// And alloc(size_t) and free(void*, size_t) are not noexcept: inlined into a caller that is not
/// either, they would bring a terminate region with them and lose the tail call into the table.
class sta_memory_pool final : public static_thread_heap<sta_memory_pool>
{
public:
    static constexpr uint32_t MaxBlockSize = 32 * 1024;
    static constexpr uint32_t PageSize = 1024 * 1024;

    /// The slot of a size class in the dispatch tables: one lzcnt, larger classes first.
    static constexpr uint32_t pool_index(uint32_t size) noexcept {
        return std::countl_zero(size - 1);
    }

    /// How many bytes a block of this size really holds: its whole class, and at least a pointer,
    /// because a free block keeps the link to the next one in its own bytes.
    static constexpr uint32_t block_size(uint32_t size) noexcept {
        ensure(size >= 1 && size <= MaxBlockSize);
        return std::max(1u << (32 - pool_index(size)), MinBlockSize);
    }

    inline static void* alloc(size_t size) {
        debug_check_thread();
        const auto size32 = static_cast<uint32_t>(size);
        assert(size32 == size);

        if (size32 <= MaxBlockSize) [[likely]]
            return s_alloc_table[pool_index(size32)]();

        if (size32 > MallocThreshold) return checked_malloc(size32);

        return base::alloc(size32);
    }

    inline static void free(void* mem, size_t size) {
        debug_check_thread();
        const auto size32 = static_cast<uint32_t>(size);
        assert(size32 == size);

        if (size32 <= MaxBlockSize) [[likely]]
            s_free_table[pool_index(size32)](mem);
        else if (size32 > MallocThreshold)
            std::free(mem);
        else
            base::free(mem);
    }

    /// alloc() and free() for a size fixed at compile time, which leaves nothing at the call site
    /// but the table load. Meant for an operator new/delete passing sizeof its own class.
    template <uint32_t Size>
        requires(Size >= 1 && Size <= MaxBlockSize)
    inline static void* alloc() noexcept {
        debug_check_thread();
        constexpr uint32_t index = pool_index(Size);
        return s_alloc_table[index]();
    }

    template <uint32_t Size>
        requires(Size >= 1 && Size <= MaxBlockSize)
    inline static void free(void* mem) noexcept {
        debug_check_thread();
        constexpr uint32_t index = pool_index(Size);
        s_free_table[index](mem);
    }

private:
    using base = static_thread_heap<sta_memory_pool>;
    using alloc_fn = void* (*)() noexcept;
    using free_fn = void (*)(void*) noexcept;

    static constexpr uint32_t MinBlockSize = sizeof(void*);
    static constexpr uint32_t MallocThreshold = PageSize / 2;

    /// One slot for every value countl_zero can give for a uint32_t, 0 to 32.
    static constexpr size_t TableSize = 33;

    static alloc_fn s_alloc_table[TableSize];
    static free_fn s_free_table[TableSize];

    template <unsigned N>
    struct pool_;

    /// Out of line, so the failure path stays out of every caller.
    static void* checked_malloc(size_t size) noexcept;

    sta_memory_pool();
    ~sta_memory_pool();

    static const sta_memory_pool s_instance;
};

template <typename T>
using sta_allocator = allocator<T, sta_memory_pool>;

}  // export namespace wxl::core
