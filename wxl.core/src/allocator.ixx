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

template <typename T, typename Al>
class allocator
{
public:
    using value_type = T;

    template <typename U>
    struct rebind {
        using other = allocator<U, Al>;
    };

    allocator() noexcept = default;

    template <typename U>
    constexpr allocator(const allocator<U, Al>&) noexcept {}

    [[nodiscard]] T* allocate(size_t n) { return static_cast<T*>(Al::alloc(n * sizeof(T))); }

    void deallocate(T* p, size_t n) noexcept {
        if (p == nullptr || n == 0) [[unlikely]]
            return;

        if constexpr (sized_free<Al>)
            Al::free(p, n * sizeof(T));
        else
            Al::free(p);
    }
};

template <typename T, typename U, typename Al>
bool operator==(const allocator<T, Al>&, const allocator<U, Al>&) noexcept {
    return true;
}

}  // namespace wxl::core
