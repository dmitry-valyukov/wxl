module;
#include "pch.h"
#include <intrin.h>

module wxl.core;
import std;

namespace wxl::core {

// The `lib` segment is initialised before the `user` one the rest of the program lands in, and
// destroyed after it: that is what puts the pool ahead of the first wxl object and behind the last.
#pragma init_seg(lib)

namespace {

// Kept on the ordinary heap: the vector is built before the pool that could serve it.
std::vector<void*> s_pages;

constinit std::byte* s_cursor = nullptr;
constinit std::byte* s_end = nullptr;

// The smallest class is two pointers wide, and the requests below it share it: with every class a
// multiple of it and the cursor starting on a page, every block comes out aligned to it, which is
// what the memory of operator new is aligned to and what a coroutine frame is laid out for.
constexpr unsigned MinN = std::countr_zero(sizeof(void*) * 2);
static_assert((1u << MinN) == sta_memory_pool::MinBlockSize);

// Moves the cursor to a new page. The rest of the old one goes onto the free lists, cut into the
// largest classes that fit.
void alloc_page() noexcept {
    const auto page = static_cast<std::byte*>(::VirtualAlloc(
        nullptr, sta_memory_pool::PageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (page == nullptr) [[unlikely]]
        abort("Page allocation failed");

    std::byte* rest = s_cursor;

    for (auto rest_size = static_cast<unsigned>(s_end - s_cursor); rest_size != 0;) {
        assert(rest_size % sta_memory_pool::MinBlockSize == 0);
        const unsigned size = std::min(std::bit_floor(rest_size), sta_memory_pool::MaxBlockSize);
        sta_memory_pool::free(rest, size);
        rest += size;
        rest_size -= size;
    }

    s_pages.push_back(page);
    s_cursor = page;
    s_end = page + sta_memory_pool::PageSize;
}

template <unsigned N>
void* bump_alloc() noexcept {
    constexpr unsigned size = (1u << N);

    std::byte* const current = s_cursor;
    std::byte* const next = current + size;

    if (next <= s_end) [[likely]] {
        s_cursor = next;
        return current;
    }

    alloc_page();
    return std::exchange(s_cursor, s_cursor + size);
}

void* invalid_alloc() noexcept { abort("Invalid allocation size (0)"); }

void invalid_free(void*) noexcept { abort("Invalid free size (0)"); }

// What a freed block holds. It is created by placement new when the block is freed, so the list
// is walked through pointers to real objects rather than through casts of raw memory.
struct free_block {
    free_block* next;
};

}  // namespace

/// Holds the free list of the size class of `1u << N` bytes. The allocation slots of the class
/// point at `get()` while the list has blocks and at `bump_alloc` once it is empty, so a class
/// that is only allocated from never tests the list.
template <unsigned N>
struct sta_memory_pool::pool_ {
    /// The slots this class answers: its own, and for the smallest class every slot below it too,
    /// so that a request smaller than a block finds the list the blocks go back to.
    static constexpr unsigned first = pool_index(1u << N);
    /// The last slot that this class answers: the last slot of the table for the smallest
    /// class, `first` for any other.
    static constexpr unsigned last = N == MinN ? TableSize - 1 : first;

    /// The head of the free list; null when the list is empty.
    static constinit inline free_block* s_top = nullptr;

    /// Sets every slot of this class in the allocation table to \p fn.
    static void dispatch(alloc_fn fn) noexcept {
        for (unsigned slot = first; slot <= last; ++slot) s_alloc_table[slot] = fn;
    }

    /// Returns the block at the head of the free list. When the list is empty, sets the slots of
    /// this class to `bump_alloc` and carves the block from the current page, or from a new page
    /// when the current one is full.
    static void* get() noexcept {
        if (free_block* const block = s_top) [[likely]] {
            s_top = block->next;
            return block;
        }

        dispatch(bump_alloc<N>);
        return bump_alloc<N>();
    }

    /// Pushes the block at \p mem onto the free list and sets the slots of this class to `get()`.
    static void put(void* mem) noexcept {
        s_top = ::new (mem) free_block{s_top};
        dispatch(get);
    }
};

// Slot 0 is a request for nothing, 1 to 16 lie above MaxBlockSize and are never read, and 17 to 32
// run from the 32k class down to a single byte.
// clang-format off
constinit sta_memory_pool::alloc_fn sta_memory_pool::s_alloc_table[TableSize] = {
    invalid_alloc,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    bump_alloc<15>, bump_alloc<14>, bump_alloc<13>, bump_alloc<12>, bump_alloc<11>,
    bump_alloc<10>, bump_alloc<9>,  bump_alloc<8>,  bump_alloc<7>,  bump_alloc<6>,
    bump_alloc<5>,  bump_alloc<4>,  bump_alloc<MinN>,
    bump_alloc<MinN>, bump_alloc<MinN>, bump_alloc<MinN>,
};

constinit sta_memory_pool::free_fn sta_memory_pool::s_free_table[TableSize] = {
    invalid_free,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    pool_<15>::put, pool_<14>::put, pool_<13>::put, pool_<12>::put,
    pool_<11>::put, pool_<10>::put, pool_<9>::put,  pool_<8>::put,
    pool_<7>::put,  pool_<6>::put,  pool_<5>::put,  pool_<4>::put,
    pool_<MinN>::put,
    pool_<MinN>::put, pool_<MinN>::put, pool_<MinN>::put,
};
// clang-format on

// The size-class index is the lzcnt instruction itself (leading_zeros): on a processor without
// it the intrinsic decodes as bsr and gives another number, so the pool refuses to start there.
// CPUID 0x80000001, ECX bit 5: ABM/LZCNT.
static void check_lzcnt() noexcept {
    int regs[4];
    __cpuid(regs, static_cast<int>(0x80000001));

    if ((regs[2] & (1 << 5)) == 0) [[unlikely]]
        abort("The processor has no lzcnt instruction");
}

sta_memory_pool::sta_memory_pool() {
    check_lzcnt();
    s_pages.reserve(64);
    alloc_page();
}

sta_memory_pool::~sta_memory_pool() {
    for (void* page : s_pages) ::VirtualFree(page, 0, MEM_RELEASE);

    s_pages.clear();
}

const sta_memory_pool sta_memory_pool::s_instance;

void* sta_memory_pool::checked_malloc(size_t size) noexcept {
    void* const mem = std::malloc(size);

    if (mem == nullptr) [[unlikely]]
        abort("Failed to allocate memory outside the pool");

    return mem;
}

}  // namespace wxl::core
