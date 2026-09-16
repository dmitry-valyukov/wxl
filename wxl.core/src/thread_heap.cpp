module;
#include "pch.h"

module wxl.core;
import std;

namespace wxl::core {

thread_heap::thread_heap() { init(); }

thread_heap::thread_heap(HANDLE heap) noexcept : heap_(heap) {}

thread_heap::~thread_heap() {
    if (heap_ != nullptr) ::HeapDestroy(heap_);
}

void thread_heap::init(size_t heap_size) noexcept {
    assert(heap_ == nullptr && "thread_heap is already initialized");

    heap_ = ::HeapCreate(HEAP_NO_SERIALIZE, heap_size, 0);

    if (heap_ == nullptr) [[unlikely]]
        abort("Failed to create thread heap");
}

void* thread_heap::alloc(size_t size) noexcept {
    assert(heap_ != nullptr && "thread_heap must be initialized before allocation");

    void* ptr = ::HeapAlloc(heap_, 0, size);

    if (ptr == nullptr) [[unlikely]]
        abort("Failed to allocate memory from thread heap");

    return ptr;
}

void thread_heap::free(void* mem) noexcept {
    if (mem == nullptr) [[unlikely]]
        return;

    assert(heap_ != nullptr && "thread_heap must be initialized before freeing memory");

    ::HeapFree(heap_, 0, mem);
}

}  // namespace wxl::core
