module;
#include "pch.h"

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

// What a freed block holds. It is created by placement new when the block is freed, so the list
// is walked through pointers to real objects rather than through casts of raw memory.
struct free_block {
    free_block* next;
};

// The smallest class is a pointer wide, and the requests below it share it.
constexpr unsigned MinLog2Size = std::countr_zero(sizeof(void*));

// Moves the cursor to a new page. The rest of the old one goes onto the free lists, cut into the
// largest classes that fit.
void alloc_page() noexcept {
    const auto page = static_cast<std::byte*>(::VirtualAlloc(
        nullptr, sta_memory_pool::PageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (page == nullptr) [[unlikely]]
        abort("Page allocation failed");

    std::byte* rest = s_cursor;

    for (auto rest_size = static_cast<uint32_t>(s_end - s_cursor); rest_size != 0;) {
        const uint32_t size = std::min(std::bit_floor(rest_size), sta_memory_pool::MaxBlockSize);
        sta_memory_pool::free(rest, size);
        rest += size;
        rest_size -= size;
    }

    s_pages.push_back(page);
    s_cursor = page;
    s_end = page + sta_memory_pool::PageSize;
}

template <unsigned Log2Size>
void* bump_alloc() noexcept {
    std::byte* const block = s_cursor;

    if (std::byte* const next = block + (1u << Log2Size); next <= s_end) [[likely]] {
        s_cursor = next;
        return block;
    }

    alloc_page();
    return std::exchange(s_cursor, s_cursor + (1u << Log2Size));
}

void* invalid_alloc() noexcept { abort("Invalid allocation size (0)"); }

void invalid_free(void*) noexcept { abort("Invalid free size (0)"); }

}  // namespace

// Holds its dispatch slot at get() while the list has blocks and at bump_alloc once it runs dry,
// so a class that is only allocated from never tests the list at all.
template <unsigned Log2Size>
struct sta_memory_pool::free_list {
    static constexpr uint32_t index = pool_index(1u << Log2Size);

    static constinit inline free_block* s_top = nullptr;

    static void* get() noexcept {
        if (free_block* const block = s_top) [[likely]] {
            s_top = block->next;
            return block;
        }

        s_alloc_table[index] = bump_alloc<Log2Size>;
        return bump_alloc<Log2Size>();
    }

    static void put(void* mem) noexcept {
        s_top = ::new (mem) free_block{s_top};
        s_alloc_table[index] = get;
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
    bump_alloc<10>, bump_alloc<9>, bump_alloc<8>, bump_alloc<7>, bump_alloc<6>,
    bump_alloc<5>, bump_alloc<4>, bump_alloc<3>,
    bump_alloc<MinLog2Size>, bump_alloc<MinLog2Size>, bump_alloc<MinLog2Size>,
};

constinit sta_memory_pool::free_fn sta_memory_pool::s_free_table[TableSize] = {
    invalid_free,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    free_list<15>::put, free_list<14>::put, free_list<13>::put, free_list<12>::put,
    free_list<11>::put, free_list<10>::put, free_list<9>::put, free_list<8>::put,
    free_list<7>::put, free_list<6>::put, free_list<5>::put, free_list<4>::put,
    free_list<3>::put,
    free_list<MinLog2Size>::put, free_list<MinLog2Size>::put, free_list<MinLog2Size>::put,
};
// clang-format on

sta_memory_pool::sta_memory_pool() {
    s_pages.reserve(64);
    alloc_page();
}

// Cached wrappers in static storage are released by the CRT after this runs, and a release reads a
// count that lives in these pages. So the registered cleanups run first, dropping those references
// while the memory is still there. The pool is not a cleanup itself: the walk reads each cell's
// link after calling it, and a cell may live in the pool.
sta_memory_pool::~sta_memory_pool() {
    module_cleanup::run_now();

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

bool sta_memory_pool::try_extend(void* mem, uint32_t size, uint32_t new_size) noexcept {
    debug_check_thread();

    if (size > MaxBlockSize || new_size > MaxBlockSize) return false;

    const uint32_t capacity = block_size(size);

    if (new_size <= capacity) return true;

    const uint32_t growth = block_size(new_size) - capacity;

    if (static_cast<std::byte*>(mem) + capacity != s_cursor || s_cursor + growth > s_end)
        return false;

    s_cursor += growth;
    return true;
}

}  // namespace wxl::core
