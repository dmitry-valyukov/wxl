module;
#include <assert.h>

export module wxl.core:thread_heap;

import :checks;
import :noncopyable;
import :thread_guard;

export namespace wxl::core {

class thread_heap : public noncopyable
{
    using HANDLE = void*;
    HANDLE heap_ = nullptr;

public:
    static constexpr size_t InitialHeapSize = 1024 * 1024;  // 1M -- what HeapCreate reserves up front

    thread_heap();                               // initializes heap with the default page size
    explicit thread_heap(HANDLE heap) noexcept;  // takes heap or nullptr to be initialized later
    ~thread_heap();

    void init(size_t heap_size = InitialHeapSize) noexcept;

    bool initialized() noexcept { return heap_ != nullptr; }

    void* alloc(size_t size) noexcept;

    void free(void* mem) noexcept;
};

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
    static bool initialized() noexcept { return s_heap != nullptr; }

    static_thread_heap() : heap_(nullptr) {
        if (initialized()) [[unlikely]]
            abort("static_thread_heap is already initialized.");

        heap_.init();
        s_heap = &heap_;
    }

    ~static_thread_heap() { s_heap = nullptr; }

    static void* alloc(size_t size) noexcept {
        thread_guard<Tag>::debug_check_thread();
        assert(initialized() && "Heap must be initialized before allocation");

        return s_heap->alloc(size);
    }

    static void free(void* mem) noexcept {
        thread_guard<Tag>::debug_check_thread();
        assert(initialized() && "Heap must be initialized before freeing memory");

        s_heap->free(mem);
    }
};

}  // export namespace wxl::core
