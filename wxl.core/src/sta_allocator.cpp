module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

// Everything with a dynamic initializer in this unit goes into the `lib`
// segment: it is initialised before the `user` segment every other object of
// the program lands in, and destroyed after it. That is what makes the pool
// below stand before the first wxl object at namespace scope is built and
// outlive the last one -- and what puts s_pages, which the pool's constructor
// reserves, ahead of the pool in the same order.
#pragma init_seg(lib)

namespace {

// Bookkeeping only (page base pointers, freed in ~sta_memory_pool), not on
// the hot allocation path the pool exists to speed up, so it deliberately
// uses the ordinary allocator rather than routing through sta_memory_pool
// itself: it is constructed (and, under the MSVC debug STL, allocates a debug
// proxy through its allocator) before the pool that would serve it.
std::vector<void*> s_pages;

using byte = std::byte;
static byte* s_cursor{};
static byte* s_end{};

byte* os_alloc_page_impl(size_t page_size) noexcept {
    return static_cast<byte*>(
        ::VirtualAlloc(nullptr, page_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}

void alloc_page() noexcept {
    // Allocate a new page
    byte* page = os_alloc_page_impl(sta_memory_pool::PageSize);
    if (!page) [[unlikely]] {
        abort("Page allocation failed");
    }

    // If there's any unused memory in the current page, release it back to the pool in largest
    // possible chunks
    if (s_cursor != nullptr && s_cursor < s_end) {
        unsigned rem = static_cast<unsigned>(s_end - s_cursor);
        byte* curr = s_cursor;

        // We want to release the memory in maximal aligned blocks not exceeding MaxBlockSize
        while (rem > 0) {
            // Get highest power of two <= both rem and MaxBlockSize
            unsigned bit = 31 - std::countl_zero(rem);
            unsigned max_pool_bit = 31 - std::countl_zero(sta_memory_pool::MaxBlockSize);
            unsigned use_bit = (bit > max_pool_bit) ? max_pool_bit : bit;
            unsigned block_size = 1u << use_bit;

            // Make sure block_size does not exceed rem (shouldn't, because of above)
            if (block_size > rem) {
                block_size = rem;
            }

            // Sanity: Should never try to free a block size of 0
            assert(block_size > 0 && block_size <= sta_memory_pool::MaxBlockSize);

            sta_memory_pool::free(curr, static_cast<uint32_t>(block_size));

            curr += block_size;
            rem -= block_size;
        }
    }

    // Set up pointers for the new page
    s_pages.push_back(page);
    s_cursor = page;
    s_end = page + sta_memory_pool::PageSize;
}

void* invalid_alloc() noexcept { abort("Invalid allocation size (0)"); }

void invalid_free(void* mem) noexcept { abort("Invalid free size (0)"); }

}  // namespace

void* sta_memory_pool::malloc_(const size_t size) noexcept {
    void* mem = std::malloc(size);

    if (mem == nullptr) [[unlikely]]
        abort("Failed to allocate memory outside the pool");

    return mem;
}

bool sta_memory_pool::try_extend(void* mem, const uint32_t size, const uint32_t new_size) noexcept {
    sta_heap::debug_check_thread();

    // Outside the pool the block belongs to another allocator entirely, and
    // neither of those hands out its own bookkeeping to be moved about.
    if (size > MaxBlockSize || new_size > MaxBlockSize) return false;

    const uint32_t capacity = block_size(size);

    if (new_size <= capacity) return true;

    const uint32_t wanted = block_size(new_size);
    byte* const block = static_cast<byte*>(mem);

    // Only the block the cursor stopped just past can grow: anything else has
    // a neighbour behind it. The page has to have the room, too -- rolling
    // over to a new page is exactly the move that would leave the block where
    // it was and the extension somewhere else.
    if (block + capacity != s_cursor || s_cursor + (wanted - capacity) > s_end) return false;

    s_cursor += wanted - capacity;

    return true;
}

sta_memory_pool::sta_memory_pool() {
    s_pages.reserve(64);
    alloc_page();
}

sta_memory_pool const sta_memory_pool::s_instance{};

sta_memory_pool::~sta_memory_pool() {
    // The pool is the last thing to go, and deliberately so. Resources that
    // cached objects allocated from it -- the generated brush and style tables
    // above all -- hold those objects in static storage that the CRT destroys
    // at process exit, after this destructor has already run. Releasing such a
    // wrapper reads the reference count in its Impl, which lives in a page this
    // loop below is about to hand back to the OS. So the pages must outlive
    // every one of those releases: run the registered cleanups first (they
    // clear the caches, dropping the last references while the memory is still
    // there), and only then free.
    //
    // Not itself registered as a `last` cleanup: execute_at_exit reads each
    // cell's `prev_` link *after* calling it (module_cleanup.cpp), so a cell
    // that freed the pool would dangle its own link mid-walk. Freeing here,
    // past the whole walk, is what keeps that safe -- and what lets a cleanup
    // cell, too, live in the pool without being read after it is gone.
    module_cleanup::run_now();
    for (void* page : s_pages) ::VirtualFree(page, 0, MEM_RELEASE);
    s_pages.clear();
}

template <unsigned N>
void* bump_alloc() noexcept {
    constexpr unsigned size = 1 << N;
    if (byte* c = s_cursor, *c1 = c + size; c1 <= s_end) [[likely]]
        return (s_cursor = c1), c;

    alloc_page();
    return std::exchange(s_cursor, s_cursor + size);
}

template <unsigned N>
struct sta_memory_pool::pool_ {
    inline static constexpr unsigned size = 1u << N;
    inline static constexpr unsigned index = pool_index(size);

    static void* get() noexcept {
        if (void* node = top_) [[likely]]
            return (top_ = *static_cast<void**>(node)), node;
        s_alloc_table[index] = bump_alloc<N>;
        return bump_alloc<N>();
    }

    static void put(void* node) noexcept {
        (*static_cast<void**>(node) = top_), top_ = node;
        s_alloc_table[index] = &pool_::get;
    }

    static void* top_;
};

template <unsigned N>
void* sta_memory_pool::pool_<N>::top_ = nullptr;

// s_alloc_table/s_free_table are indexed by pool_index(size) == std::countl_zero(size - 1).
// For any size in [1, MaxBlockSize] that index ranges over [17, 32]
// (idx 17 <-> the [16385, 32768] class, idx 32 <-> the [1, 1] class), so
// exactly those 16 slots must be populated; idx 1..16 are unreachable from
// alloc_impl/free_impl (guarded by `size <= MaxBlockSize`) and stay
// nullptr, and idx 0 is reserved for the invalid size == 0 case.
sta_memory_pool::alloc_fn sta_memory_pool::s_alloc_table[33] = {
    invalid_alloc, nullptr,    nullptr,    nullptr,    nullptr,    nullptr,
    nullptr,       nullptr,    nullptr,    nullptr,    nullptr,    nullptr,
    nullptr,        nullptr,        nullptr,        nullptr,
    nullptr,        bump_alloc<15>, bump_alloc<14>, bump_alloc<13>,
    bump_alloc<12>, bump_alloc<11>, bump_alloc<10>, bump_alloc<9>,
    bump_alloc<8>,  bump_alloc<7>,  bump_alloc<6>,  bump_alloc<5>,
    bump_alloc<4>,  bump_alloc<3>,
#ifdef PLATFORM_64BIT
    bump_alloc<03>, bump_alloc<03>, bump_alloc<03>,
#else
    bump_alloc<02>, bump_alloc<02>, bump_alloc<02>,
#endif
};

sta_memory_pool::free_fn sta_memory_pool::s_free_table[33] = {
    invalid_free,    nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         nullptr,         nullptr,         nullptr,
    nullptr,         nullptr,         &pool_<15>::put, &pool_<14>::put, &pool_<13>::put,
    &pool_<12>::put, &pool_<11>::put, &pool_<10>::put, &pool_<9>::put,  &pool_<8>::put,
    &pool_<7>::put,  &pool_<6>::put,  &pool_<5>::put,  &pool_<4>::put,  &pool_<3>::put,
#ifdef PLATFORM_64BIT
    &pool_<03>::put, &pool_<03>::put, &pool_<03>::put,
#else
    &pool_<02>::put, &pool_<02>::put, &pool_<02>::put,
#endif
};

}  // namespace wxl::core
