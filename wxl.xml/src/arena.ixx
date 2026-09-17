// The storage a parsed tree is made of: its nodes, their attribute arrays and
// the values assembled out of pieces.
//
// A tree is built once, read many times and dies whole, so nothing in it is
// ever released on its own. Memory comes in pages, objects are carved out by
// moving a cursor, and the lot goes back in one pass -- no per-object header,
// no free list, and no destructor call, which is why only trivially
// destructible types may be put here.
//
// An arena is built when there is something to put in it and dies with it:
// there is no empty arena and no reused one. That is what leaves the hot path
// with nothing to test -- the first page is taken in the constructor, so the
// cursor always points at real memory, and an allocation is a rounding, a
// compare and a store. Everything a document is done with goes away by
// dropping its arena, which is also the only place the pages go back.
//
// The pages come from arena_pool and go back to it: one size, kept for the
// life of the process, so the second book of a session is built in the first
// book's pages, warm. The one thing that does not come from there is a single
// request too big for a page -- a value the markup tore into pieces and the
// arena has to put back together -- which gets a page of its own from the
// ordinary allocator, where a lone huge object belongs.

export module wxl.xml:arena;

import :core;

export namespace wxl::xml {

class arena {
public:
    /// Takes its first page. An arena with no page in it is a state this class
    /// does not have, and the constructor is where that is settled.
    arena();

    /// Hands every page back. Everything the arena ever gave out dangles from
    /// here on, which is the point: a tree dies in one move.
    ~arena();

    arena(const arena&) = delete;
    arena& operator=(const arena&) = delete;

    /// One arena per parse, and parses repeat -- a chat feed parses per
    /// message -- so the arena itself comes out of the STA pool, the way
    /// wxl.html's per-parse state does.
    inline static void* operator new(std::size_t size) {
        return core::sta_memory_pool::alloc(size);
    }

    inline static void operator delete(void* ptr, std::size_t size) noexcept {
        core::sta_memory_pool::free(ptr, size);
    }

    /// Builds one T in the arena. The T lives as long as the arena.
    template <typename T, typename... Args>
    T* create(Args&&... args) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "the arena never runs a destructor, so it only holds types that do not need one");

        return ::new (allocate<alignof(T)>(sizeof(T))) T(std::forward<Args>(args)...);
    }

    /// Copies a sequence into the arena and hands back the span over the copy.
    template <typename T>
    std::span<const T> copy(std::span<const T> items) {
        static_assert(std::is_trivially_destructible_v<T>,
                      "the arena never runs a destructor, so it only holds types that do not need one");

        if (items.empty()) [[unlikely]]
            return {};

        auto* const memory = static_cast<T*>(allocate<alignof(T)>(items.size_bytes()));

        // Fresh arena memory never overlaps what it is copied from, so this is
        // memcpy rather than the memmove uninitialized_copy comes down to for
        // a trivially copyable T.
        if constexpr (std::is_trivially_copyable_v<T>)
            std::memcpy(memory, items.data(), items.size_bytes());
        else
            std::uninitialized_copy(items.begin(), items.end(), memory);

        return {memory, items.size()};
    }

    /// Copies text into the arena and hands back the view over the copy. The
    /// arena never moves what it has handed out, so that view stays put for
    /// the tree's whole life.
    inline std::string_view copy(const std::string_view text) {
        if (text.empty()) [[unlikely]]
            return {};

        const std::span<const char> copied = copy(std::span<const char>(text));

        return {copied.data(), copied.size()};
    }

    /// Room for `count` bytes, uninitialised, for a caller that writes them
    /// itself.
    inline std::span<char> chars(const std::size_t count) {
        if (count == 0) [[unlikely]]
            return {};

        return {static_cast<char*>(allocate<1>(count)), count};
    }

private:
    /// The whole of the hot path, and here rather than in the implementation
    /// unit so that it lands inside the call that carves a node: round the
    /// cursor up, move it on, hand back where it stood. No page to check for
    /// and no null to test -- the constructor saw to that -- and no division
    /// either, since rounding up to a power of two is a mask.
    ///
    /// The alignment is a template parameter and not an argument because it is
    /// never anything but alignof at the call site. That makes the mask a
    /// constant where no optimizer is running to fold one -- the debug build
    /// inlines nothing at all -- and for a run of bytes it is not written:
    /// `if constexpr` leaves the rounding out of the code rather than leaving
    /// it to be simplified away.
    ///
    /// The compare is in addresses rather than pointers because the rounded
    /// cursor may land past the page, which is a question worth asking but not
    /// a pointer worth forming.
    template <std::size_t Alignment>
    [[nodiscard]] [[msvc::forceinline]] void* allocate(const std::size_t size) {
        static_assert(Alignment != 0 && (Alignment & (Alignment - 1)) == 0,
                      "the alignment comes from alignof and is therefore a power of two");

        std::uintptr_t at = reinterpret_cast<std::uintptr_t>(cursor_);

        if constexpr (Alignment > 1)
            at = (at + (Alignment - 1)) & ~static_cast<std::uintptr_t>(Alignment - 1);

        if (at + size <= reinterpret_cast<std::uintptr_t>(end_)) [[likely]] {
            cursor_ = reinterpret_cast<std::byte*>(at + size);

            return reinterpret_cast<void*>(at);
        }

        return allocate_slow(size, Alignment);
    }

    /// What the page in hand cannot answer: the next page, or a page cut to
    /// measure for a request no page would hold. Out of line, and the only
    /// reason the hot path above is short enough to inline.
    void* allocate_slow(std::size_t size, std::size_t alignment);

    /// Takes a page from the pool and starts carving from it.
    void alloc_page();

    /// Puts a page at the head of the chain and answers with its payload. The
    /// cursor is left alone, which is what lets a page cut to measure be
    /// chained the same way while the page in hand goes on serving.
    std::byte* link(std::byte* memory, std::uint32_t bytes) noexcept;

    /// One page, with the next chained off it, and how long it is. The length
    /// also says where it came from -- a page from the pool is always its one
    /// size, anything else was cut to measure -- so nothing more has to be
    /// written down for the destructor to give each one back to the right
    /// place.
    struct page {
        page* next;
        std::uint32_t size;
    };

    static constexpr std::size_t header_size = sizeof(page);

    // The two the hot path reads come first, and together: every allocation
    // touches both and nothing else. Neither is initialised here -- the
    // constructor takes a page, and there is no state in which they are null.
    std::byte* cursor_;
    std::byte* end_;
    page* pages_ = nullptr;
};

}  // namespace wxl::xml
