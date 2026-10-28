module;
#include <assert.h>

export module wxl.core:thread_heap;

import :checks;
import :noncopyable;
import :thread_guard;

export namespace wxl::core {

/// Represents a private Windows heap created with `HEAP_NO_SERIALIZE`, for use from one thread.
/// The object owns the heap and destroys it in its destructor.
class thread_heap : public noncopyable
{
    using HANDLE = void*;
    HANDLE heap_ = nullptr;

public:
    /// The initial size of the heap, in bytes, that `init()` passes to `HeapCreate` by default:
    /// 1 MiB.
    static constexpr size_t InitialHeapSize = 1024 * 1024;

    /// Constructs a thread heap and creates the Windows heap with the default initial size.
    thread_heap();
    /// Constructs a thread heap that takes ownership of \p heap; with a null \p heap, the
    /// Windows heap is created later by `init()`.
    explicit thread_heap(HANDLE heap) noexcept;
    ~thread_heap();

    /// Creates the Windows heap with an initial size of \p heap_size bytes. The heap must not
    /// exist yet; a failure to create it aborts the process.
    void init(size_t heap_size = InitialHeapSize) noexcept;

    /// Tests whether the Windows heap exists.
    inline bool initialized() noexcept { return heap_ != nullptr; }

    /// Allocates \p size bytes from the heap; a failure aborts the process.
    void* alloc(size_t size) noexcept;

    /// Releases the block at \p mem to the heap; a null \p mem is ignored.
    void free(void* mem) noexcept;
};

/// Represents the one heap of `Tag`, reached through static functions. The instance owns a
/// `wxl::core::thread_heap` and is bound by `wxl::core::thread_guard` to the thread that
/// constructs it; constructing a second instance aborts the process.
template <typename Tag>
class static_thread_heap : public thread_guard<Tag>
{
    // The heap is a member of the one instance, and this points at it. A
    // static heap of its own would be built and destroyed in the `user`
    // segment's order -- after an instance constructed in the `lib` segment
    // had initialised it, and before that instance's destructor -- so the
    // instance owns the heap, and the static is only the address.
    static constinit inline thread_heap* s_heap = nullptr;

    thread_heap heap_;

public:
    /// Tests whether the instance exists.
    static bool initialized() noexcept { return s_heap != nullptr; }

    static_thread_heap() : heap_(nullptr) {
        if (initialized()) [[unlikely]]
            abort("static_thread_heap is already initialized.");

        heap_.init();
        s_heap = &heap_;
    }

    ~static_thread_heap() { s_heap = nullptr; }

    /// Allocates \p size bytes from the heap of the instance, which must exist.
    static void* alloc(size_t size) noexcept {
        thread_guard<Tag>::debug_check_thread();
        assert(initialized() && "Heap must be initialized before allocation");

        return s_heap->alloc(size);
    }

    /// Releases the block at \p mem to the heap of the instance, which must exist.
    static void free(void* mem) noexcept {
        thread_guard<Tag>::debug_check_thread();
        assert(initialized() && "Heap must be initialized before freeing memory");

        s_heap->free(mem);
    }
};

}  // export namespace wxl::core
