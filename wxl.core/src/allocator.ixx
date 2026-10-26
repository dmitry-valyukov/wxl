export module wxl.core:allocator;

import wxl.stdint;

export namespace wxl::core {

// Some allocator backends (e.g. sta_memory_pool) need the block size to route
// the free between size classes; others (e.g. thread_heap, backed directly by
// the Windows heap) don't. deallocate() picks whichever `Al::free` overload
// is actually available instead of requiring every backend to accept a size
// it has no use for.
template <typename Al>
concept sized_free = requires(void* p, size_t n) { Al::free(p, n); };

/// Provides a standard allocator of `T` over the static functions `Al::alloc` and `Al::free`.
/// The allocator holds no state.
template <typename T, typename Al>
class allocator
{
public:
    /// The type of the allocated objects.
    using value_type = T;

    /// Provides the allocator of another object type over the same backend.
    template <typename U>
    struct rebind {
        /// The type of the allocator of `U` over `Al`.
        using other = allocator<U, Al>;
    };

    allocator() noexcept = default;

    /// Constructs an allocator from an allocator of another object type over the same backend.
    template <typename U>
    constexpr allocator(const allocator<U, Al>&) noexcept {}

    /// Allocates storage for \p n objects of type `T` from `Al`.
    [[nodiscard]] T* allocate(size_t n) { return static_cast<T*>(Al::alloc(n * sizeof(T))); }

    /// Releases the storage for \p n objects at \p p to `Al`, passing the size in bytes when
    /// `Al::free` accepts one. A null \p p or a zero \p n is ignored.
    void deallocate(T* p, size_t n) noexcept {
        if (p == nullptr || n == 0) [[unlikely]]
            return;

        if constexpr (sized_free<Al>)
            Al::free(p, n * sizeof(T));
        else
            Al::free(p);
    }
};

/// Tests whether two allocators over the same backend are equal; they always are.
template <typename T, typename U, typename Al>
bool operator==(const allocator<T, Al>&, const allocator<U, Al>&) noexcept {
    return true;
}

}  // namespace wxl::core
