module;
#include "pch.h"

module wxl.xml;

import std;
import :arena_pool;

namespace wxl::xml {

using namespace ::wxl::core;
using byte = std::byte;

namespace {
std::vector<void*> s_pages;

static byte* s_cursor{};
static byte* s_end{};

byte* os_alloc_page_impl(size_t page_size) noexcept {
    return static_cast<byte*>(
        ::VirtualAlloc(nullptr, page_size, MEM_COMMIT | MEM_RESERVE, PAGE_READWRITE));
}

constexpr size_t const s_page_size = 1024*1024;

void alloc_page() noexcept {
    byte* page = os_alloc_page_impl(s_page_size);
    if (!page) [[unlikely]] {
        abort("Page allocation failed");
    }
    s_pages.push_back(page);
    s_cursor = page;
    s_end = page + s_page_size;
}

void free_pages() {
    for (void* page : s_pages) ::VirtualFree(page, 0, MEM_RELEASE);
    s_pages.clear();
}

module_cleanup s_cleanup(free_pages, module_cleanup::priority::cleanup_last);

void* invalid_alloc() noexcept { abort("Invalid allocation size (0)"); }

void invalid_free(void* mem) noexcept { abort("Invalid free size (0)"); }

struct initializer_{
    initializer_() {
        s_pages.reserve(64);
        alloc_page();
    }
} init;

}  // namespace

byte* bump_alloc() noexcept {
    if (byte* c = s_cursor, *c1 = c + arena_pool::block_size; c1 <= s_end) [[likely]]
        return (s_cursor = c1), c;

    alloc_page();
    return std::exchange(s_cursor, s_cursor + arena_pool::block_size);
}

byte* pool_top_ = nullptr;

byte* arena_pool::pool_get() noexcept {
    if (byte* node = pool_top_) [[likely]]
        return (pool_top_ = *std::bit_cast<byte**>(node)), node;
    s_alloc_table = bump_alloc;
    return bump_alloc();
}

void arena_pool::free  (byte* block) noexcept {
    (*std::bit_cast<void**>(block) = static_cast<void*>(pool_top_)), pool_top_ = block;
    s_alloc_table = pool_get;
}

arena_pool::alloc_fn arena_pool::s_alloc_table = bump_alloc;

}  // namespace wxl::xml
