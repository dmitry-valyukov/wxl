// windows.h is used for real here: the independence of two heaps is proven
// through ::VirtualQuery and MEMORY_BASIC_INFORMATION.
#include "platform.h"

#include <gtest/gtest.h>

import std;
import wxl.core;

namespace {

using wxl::core::static_thread_heap;
using wxl::core::thread_heap;

void expect_usable_memory(void* mem, size_t size) {
    ASSERT_NE(mem, nullptr);

    std::memset(mem, 0xAB, size);
    EXPECT_EQ(static_cast<unsigned char*>(mem)[0], 0xAB);
    EXPECT_EQ(static_cast<unsigned char*>(mem)[size - 1], 0xAB);
}

void expect_independent_allocations(void* a, void* b, size_t size) {
    ASSERT_NE(a, nullptr);
    ASSERT_NE(b, nullptr);
    EXPECT_NE(a, b);

    std::memset(a, 0xAA, size);
    std::memset(b, 0xBB, size);

    EXPECT_EQ(static_cast<unsigned char*>(a)[0], 0xAA) << "first allocation was overwritten";
    EXPECT_EQ(static_cast<unsigned char*>(b)[0], 0xBB) << "second allocation was overwritten";
}

TEST(ThreadHeapTest, DefaultConstructedHeapIsInitialized) {
    thread_heap heap;
    EXPECT_TRUE(heap.initialized());
}

TEST(ThreadHeapTest, NullHeapIsNotInitializedUntilInit) {
    thread_heap heap(nullptr);
    EXPECT_FALSE(heap.initialized());

    heap.init();
    EXPECT_TRUE(heap.initialized());
}

TEST(ThreadHeapTest, InitAcceptsACustomPageSize) {
    thread_heap heap(nullptr);
    heap.init(64 * 1024);
    EXPECT_TRUE(heap.initialized());

    void* mem = heap.alloc(256);
    expect_usable_memory(mem, 256);
    heap.free(mem);
}

TEST(ThreadHeapTest, AllocatedMemoryIsUsable) {
    thread_heap heap;

    void* mem = heap.alloc(128);
    expect_usable_memory(mem, 128);

    heap.free(mem);
}

TEST(ThreadHeapTest, AllocationsDoNotOverlap) {
    thread_heap heap;

    void* a = heap.alloc(64);
    void* b = heap.alloc(64);

    expect_independent_allocations(a, b, 64);

    heap.free(a);
    heap.free(b);
}

TEST(ThreadHeapTest, FreeingNullptrIsNoop) {
    thread_heap heap;
    heap.free(nullptr);
}

TEST(ThreadHeapTest, SeparateInstancesAreIndependent) {
    thread_heap heap_a;
    thread_heap heap_b;

    void* a = heap_a.alloc(64);
    void* b = heap_b.alloc(64);

    expect_independent_allocations(a, b, 64);

    heap_a.free(a);
    heap_b.free(b);
}

// static_thread_heap<Tag> just forwards to a thread_heap instance shared per
// Tag, so a single typed-test body run for a couple of distinct tags covers
// it without duplicating the same assertions by hand for every tag. Each Tag
// may only be constructed once for the lifetime of the process (see
// static_thread_heap's ctor), so every scenario for a given tag has to live
// inside a single test body below rather than being split across several
// TYPED_TESTs.
template <typename Tag>
class StaticThreadHeapTest : public ::testing::Test {};

struct TagA {};
struct TagB {};

using StaticThreadHeapTags = ::testing::Types<TagA, TagB>;
TYPED_TEST_SUITE(StaticThreadHeapTest, StaticThreadHeapTags);

TYPED_TEST(StaticThreadHeapTest, ConstructsAllocatesAndFreesIndependently) {
    using heap = static_thread_heap<TypeParam>;

    heap h;
    EXPECT_TRUE(h.initialized());

    void* a = heap::alloc(64);
    void* b = heap::alloc(64);
    expect_independent_allocations(a, b, 64);

    heap::free(a);
    heap::free(b);
    heap::free(nullptr);
}

// Two tags never touched by the typed-test suite above, used to show that
// static_thread_heap keeps a fully separate heap per Tag rather than sharing
// state across unrelated consumers.
struct IsolationTagX {};
struct IsolationTagY {};

TEST(StaticThreadHeapTest, DifferentTagsGetIndependentHeaps) {
    using heap_x = static_thread_heap<IsolationTagX>;
    using heap_y = static_thread_heap<IsolationTagY>;

    heap_x x;
    heap_y y;

    EXPECT_TRUE(x.initialized());
    EXPECT_TRUE(y.initialized());

    void* mem_x = heap_x::alloc(64);
    void* mem_y = heap_y::alloc(64);
    expect_independent_allocations(mem_x, mem_y, 64);

    heap_x::free(mem_x);
    heap_y::free(mem_y);
}

// Fresh tags, disjoint from every other test above: each static_thread_heap<Tag>
// owns its own Windows heap (HeapCreate reserves its own address range via
// VirtualAlloc), so two tags that are alive at the same time must allocate
// from different memory regions/pages, never share one.
struct PageIsolationTagA {};
struct PageIsolationTagB {};

TEST(StaticThreadHeapTest, DifferentTagsAllocateFromDifferentPages) {
    using heap_a = static_thread_heap<PageIsolationTagA>;
    using heap_b = static_thread_heap<PageIsolationTagB>;

    heap_a a;
    heap_b b;
    ASSERT_TRUE(a.initialized());
    ASSERT_TRUE(b.initialized());

    void* mem_a = heap_a::alloc(64);
    void* mem_b = heap_b::alloc(64);
    expect_independent_allocations(mem_a, mem_b, 64);

    MEMORY_BASIC_INFORMATION info_a{};
    MEMORY_BASIC_INFORMATION info_b{};
    ASSERT_NE(::VirtualQuery(mem_a, &info_a, sizeof(info_a)), 0u);
    ASSERT_NE(::VirtualQuery(mem_b, &info_b, sizeof(info_b)), 0u);

    // AllocationBase identifies the memory region a VirtualAlloc reservation
    // came from; two independent heaps must never share one.
    EXPECT_NE(info_a.AllocationBase, info_b.AllocationBase);

    heap_a::free(mem_a);
    heap_b::free(mem_b);
}

}  // namespace
