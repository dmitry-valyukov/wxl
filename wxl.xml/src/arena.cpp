module wxl.xml;

import std;
import :arena;
import :arena_pool;

namespace wxl::xml {
namespace {

/// Rounds an address up to an alignment, which is always a power of two: it
/// comes from alignof and from nowhere else.
std::byte* align_up(std::byte* const at, const std::size_t alignment) noexcept {
    const auto address = reinterpret_cast<std::uintptr_t>(at);

    return reinterpret_cast<std::byte*>((address + alignment - 1) &
                                        ~(static_cast<std::uintptr_t>(alignment) - 1));
}

}  // namespace

arena::arena() {
    // The first page is taken here rather than at the first allocation, and
    // that is what the hot path is built on: an arena exists because something
    // is about to go into it, so a page it never uses is not a case worth a
    // branch on every carve.
    alloc_page();
}

arena::~arena() {
    // The whole tree in one pass down the chain: no per-object header to walk,
    // no destructor to run. Nothing is put back in order afterwards -- an
    // arena that has let go of its pages has nothing left to be, which is why
    // this is the only place it happens.
    for (page* p = pages_; p != nullptr;) {
        page* const next = p->next;
        const std::uint32_t size = p->size;
        auto* const memory = reinterpret_cast<std::byte*>(p);

        if (size == arena_pool::block_size) [[likely]]
            arena_pool::free(memory);
        else
            std::free(memory);

        p = next;
    }
}

std::byte* arena::link(std::byte* const memory, const std::uint32_t bytes) noexcept {
    auto* const p = reinterpret_cast<page*>(memory);

    p->next = pages_;
    p->size = bytes;
    pages_ = p;

    return memory + header_size;
}

void arena::alloc_page() {
    std::byte* const payload = link(arena_pool::alloc(), arena_pool::block_size);

    cursor_ = payload;
    end_ = payload + (arena_pool::block_size - header_size);
}

void* arena::allocate_slow(const std::size_t size, const std::size_t alignment) {
    // What a page of its own would have to hold: the header, the worst the
    // payload can be pushed by, and the bytes themselves.
    const std::size_t wanted = header_size + alignment - 1 + size;

    // A request a whole page would not hold gets a page cut to measure,
    // straight from malloc. Not through the STA pool: its classes end at
    // 32 KiB and above them it only forwards, and a megabyte costs the same
    // either way -- 14.7 us against 14.9, measured -- because the heap sends a
    // block that size to the kernel whoever asks. A pool is for many small
    // objects; a buffer whose length the document chooses is not one.
    //
    // malloc rather than ::operator new, and that is not a detail: the global
    // operators are a replacement point, and a program that replaces them
    // would decide, from outside, where this block comes from. What is wanted
    // here is raw bytes from the C allocator and nothing else.
    //
    // The page in hand stays the one the cursor works from: what is left in it
    // outlives this single request, and a page cut to measure has nothing
    // left over anyway.
    if (wanted > arena_pool::block_size) {
        const auto bytes = static_cast<std::uint32_t>(wanted);
        auto* const memory = static_cast<std::byte*>(std::malloc(bytes));

        if (memory == nullptr) [[unlikely]]
            throw std::bad_alloc();

        return align_up(link(memory, bytes), alignment);
    }

    // Otherwise the next page, which by the arithmetic above has room: the
    // carve below cannot fail.
    alloc_page();

    std::byte* const at = align_up(cursor_, alignment);
    cursor_ = at + size;

    return at;
}

}  // namespace wxl::xml
