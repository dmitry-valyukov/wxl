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

// The smallest class is a pointer wide, and the requests below it share it.
constexpr unsigned MinN = std::countr_zero(sizeof(void*));

// Moves the cursor to a new page. The rest of the old one goes onto the free lists, cut into the
// largest classes that fit.
void alloc_page() noexcept {
    const auto page = static_cast<std::byte*>(::VirtualAlloc(
        nullptr, sta_memory_pool::PageSize, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));

    if (page == nullptr) [[unlikely]]
        abort("Page allocation failed");

    std::byte* rest = s_cursor;

    for (auto rest_size = static_cast<unsigned>(s_end - s_cursor); rest_size != 0;) {
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

// Holds its dispatch slot at get() while the list has blocks and at bump_alloc once it runs dry,
// so a class that is only allocated from never tests the list at all.
template <unsigned N>
struct sta_memory_pool::pool_ {
    static constexpr unsigned index = pool_index(1u << N);

    static constinit inline free_block* s_top = nullptr;

    static void* get() noexcept {
        if (free_block* const block = s_top) [[likely]] {
            s_top = block->next;
            return block;
        }

        s_alloc_table[index] = bump_alloc<N>;
        return bump_alloc<N>();
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
    bump_alloc<10>, bump_alloc<9>,  bump_alloc<8>,  bump_alloc<7>,  bump_alloc<6>,
    bump_alloc<5>,  bump_alloc<4>,  bump_alloc<3>,
    bump_alloc<MinN>, bump_alloc<MinN>, bump_alloc<MinN>,
};

constinit sta_memory_pool::free_fn sta_memory_pool::s_free_table[TableSize] = {
    invalid_free,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
    pool_<15>::put, pool_<14>::put, pool_<13>::put, pool_<12>::put,
    pool_<11>::put, pool_<10>::put, pool_<9>::put,  pool_<8>::put,
    pool_<7>::put,  pool_<6>::put,  pool_<5>::put,  pool_<4>::put,
    pool_<3>::put,
    pool_<MinN>::put, pool_<MinN>::put, pool_<MinN>::put,
};
// clang-format on

sta_memory_pool::sta_memory_pool() {
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

bool sta_memory_pool::try_extend(void* mem, unsigned size, unsigned new_size) noexcept {
    debug_check_thread();

    if (size > MaxBlockSize || new_size > MaxBlockSize) return false;

    const unsigned capacity = block_size(size);

    if (new_size <= capacity) return true;

    const unsigned growth = block_size(new_size) - capacity;

    if (static_cast<std::byte*>(mem) + capacity != s_cursor || s_cursor + growth > s_end)
        return false;

    s_cursor += growth;
    return true;
}

}  // namespace wxl::core
