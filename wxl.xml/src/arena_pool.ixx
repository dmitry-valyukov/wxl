export module wxl.xml:arena_pool;

import :core;

export namespace wxl::xml {

class arena_pool {
public:
    static constexpr std::uint32_t block_size = 64 * 1024;

    [[nodiscard]] inline static std::byte* alloc() noexcept {
        core::sta_memory_pool::debug_check_thread();
        return s_alloc_table();
    }

    static void free(std::byte* block) noexcept;

private:
    static std::byte* pool_get() noexcept;

    typedef std::byte* (*alloc_fn)() noexcept;
    static alloc_fn s_alloc_table;
};

}  // namespace wxl::xml
