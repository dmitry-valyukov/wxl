module;

#include "abi.h"

export module wxl.core:sta_allocator;

import :allocator;
import :checks;
import :thread_heap;
import std;

export namespace wxl::core {

template <typename T>
using sta_allocator = allocator<T, class sta_memory_pool>;

/// Strings whose characters live in the STA pool.
///
/// Spelled out here, next to the allocator, because the alternative is every
/// module writing the same three lines: the type is the same one whoever
/// declares it, and two spellings of it would only be two names for one thing.
///
/// Both are as thread-bound as the pool is: made, grown and destroyed on its
/// thread, and nowhere else.
///@{
using sta_string = std::basic_string<char, std::char_traits<char>, sta_allocator<char>>;

using sta_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, sta_allocator<wchar_t>>;
///@}

/// A pool for many small, short-lived objects on the one STA thread.
///
/// There is one per process and nobody builds it: the instance is a static of
/// this class, initialised in the `lib` segment -- before any object of the
/// program, on the thread the CRT starts on, which is the STA thread of every
/// wxl application -- and destroyed after the last of them. So a wxl object
/// at namespace scope is born into a living pool and dies before it, and the
/// pool's destructor, which runs the registered cleanups and gives the pages
/// back, is the last thing to go.
///
/// The thread is claimed when the pool is built, and every entry point below
/// asserts, in debug builds, that it is called from that same thread: the
/// free lists are plain, non-interlocked pointers, so a call from any other
/// thread corrupts them silently.
///
/// Two things follow from "small", and a caller has to know both. Sizes up to
/// MaxBlockSize are served from free lists carved out of pages, which is
/// what the pool exists for. Anything larger than half a page is handed to the
/// ordinary allocator, because a block that size can never share a page with
/// another one. So a buffer whose length is decided by an input rather than by
/// the program gains nothing here, and is better allocated as itself.
class sta_memory_pool final : public static_thread_heap<sta_memory_pool> {
public:
    inline static constexpr unsigned MaxBlockSize = 1u << 15;  // 32k
    inline static constexpr unsigned PageSize = 1024 * 1024;

    using sta_heap = static_thread_heap<sta_memory_pool>;

    static constexpr uint32_t pool_index(uint32_t size) noexcept {
        return std::countl_zero(size - 1);
    }

    /// How many bytes a block of this size actually holds. The pool hands out
    /// whole size classes, so a request for 100 gets 128 -- and a buffer that
    /// asks can fill what it was already given instead of growing into a new
    /// block and copying itself there.
    ///
    /// The smallest classes are a pointer wide whatever the request: a free
    /// block stores the link to the next one in its own bytes.
    static constexpr uint32_t block_size(uint32_t size) noexcept {
        ensure(size >= 1 && size <= MaxBlockSize);

        const uint32_t whole = 1u << (32 - pool_index(size));

        return whole < sizeof(void*) ? static_cast<uint32_t>(sizeof(void*)) : whole;
    }

    /// Grows a block where it stands, leaving what is in it untouched. True if
    /// the block now holds at least new_size bytes, and it must then be freed
    /// with new_size (or any size in the same class) rather than the old one.
    ///
    /// Two ways it can succeed, and both are why a growing buffer is worth
    /// building on this pool at all. The block may already be big enough,
    /// because size classes are wider than the requests that land in them. Or
    /// the block may be the last one the bump cursor handed out, in which case
    /// the free space behind it is still free and the cursor simply moves
    /// further -- which is the case a buffer that formats into itself hits
    /// every time, since nothing else allocates in between.
    ///
    /// False means only that it could not be done in place. The caller then
    /// does what it would have done anyway: allocate, copy, free.
    static bool try_extend(void* mem, uint32_t size, uint32_t new_size) noexcept;

    inline static void* alloc(size_t size) {
        sta_heap::debug_check_thread();
        const uint32_t u32size = static_cast<uint32_t>(size);
        assert(u32size == size);
        return alloc_impl(u32size);
    }

    inline static void free(void* mem, size_t size) {
        sta_heap::debug_check_thread();
        const uint32_t u32size = static_cast<uint32_t>(size);
        assert(u32size == size);
        free_impl(mem, u32size);
    }

    /// Compile-time-sized counterpart of alloc(size_t)/free(void*, size_t): Size being a
    /// template argument (rather than a runtime std::size_t, however "known" it is to the
    /// caller) lets pool_index(Size) be evaluated as a genuine constant expression -- the
    /// `constexpr` locals below force that, the same way an ordinary call with a literal
    /// argument does not. The only work left at the call site is the table lookup itself,
    /// through a compile-time-fixed offset. Meant to be reached via a CRTP operator new/delete
    /// (each derived type gets sizeof(Derived) as Size for free), not called with a
    /// runtime-computed argument -- that would defeat the point.
    template <uint32_t Size>
    inline static void* alloc() noexcept {
        static_assert(Size >= 1 && Size <= MaxBlockSize, "sta_memory_pool: Size out of pool range");
        sta_heap::debug_check_thread();
        constexpr uint32_t idx = pool_index(Size);
        return s_alloc_table[idx]();
    }

    template <uint32_t Size>
    inline static void free(void* mem) noexcept {
        static_assert(Size >= 1 && Size <= MaxBlockSize, "sta_memory_pool: Size out of pool range");
        sta_heap::debug_check_thread();
        constexpr uint32_t idx = pool_index(Size);
        s_free_table[idx](mem);
    }

private:
    inline static void* alloc_impl(uint32_t size) {
        if (size <= MaxBlockSize) [[likely]]
            return s_alloc_table[pool_index(size)]();

        // A block larger than half a page can never be packed with another
        // one, so the pool gains nothing by owning it and would only tie up a
        // page it cannot fill: it goes to the ordinary allocator instead, and
        // the pool keeps holding only what it can pack.
        if (size > MallocThreshold)
            return malloc_(size);

        return sta_heap::alloc(size);
    }

    inline static void free_impl(void* mem, uint32_t size) {
        if (size <= MaxBlockSize) [[likely]]
            return s_free_table[pool_index(size)](mem);

        // The same size the block was asked for, so this picks the same side
        // of the threshold that alloc_impl did.
        if (size > MallocThreshold)
            return std::free(mem);

        sta_heap::free(mem);
    }

    /// std::malloc, with the failure treated the way every other allocation
    /// failure here is. Out of line because that failure path has no business
    /// being inlined into a call site.
    static void* malloc_(size_t size) noexcept;

    /// Half a page, above which a block goes to the ordinary allocator.
    inline static constexpr uint32_t MallocThreshold = PageSize / 2;

    // The one instance, defined in sta_allocator.cpp in the `lib` segment.
    // Private on both ends: nothing else may build a second pool, and nothing
    // else destroys this one.
    sta_memory_pool();
    ~sta_memory_pool();

    static sta_memory_pool const s_instance;

    using alloc_fn = void* (*)() noexcept;
    using free_fn = void (*)(void*) noexcept;

    static alloc_fn s_alloc_table[33];
    static free_fn s_free_table[33];

    template <unsigned N>
    struct pool_;
};

}  // export namespace wxl::core
