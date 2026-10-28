export module wxl.core:xml.arena_pool;

import :sta_allocator;
import std;

export namespace wxl::xml {

/// Provides the fixed-size blocks that arenas are built from. Blocks are carved from pages of
/// `VirtualAlloc` memory; a released block goes onto a free list, which the next allocation
/// takes from first. The pages are returned to the system at module cleanup.
class arena_pool {
public:
    /// The size of a block, in bytes.
    static constexpr std::uint32_t block_size = 64 * 1024;

    /// Allocates one block of `block_size` bytes, from the free list if it holds one.
    /// \note In a build without `NDEBUG`, the call asserts that it is made on the thread of
    ///       the STA memory pool.
    [[nodiscard]] inline static std::byte* alloc() noexcept {
        core::sta_memory_pool::debug_check_thread();
        return s_alloc_table();
    }

    /// Releases \p block onto the free list, from which the next `alloc()` takes it.
    static void free(std::byte* block) noexcept;

private:
    static std::byte* pool_get() noexcept;

    typedef std::byte* (*alloc_fn)() noexcept;
    static alloc_fn s_alloc_table;
};

}  // namespace wxl::xml
