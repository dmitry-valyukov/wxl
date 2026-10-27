#include <gtest/gtest.h>

import std;
import wxl.core;

namespace {

using wxl::core::sta_allocator;
using wxl::core::sta_memory_pool;

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

TEST(StaMemoryPoolTest, SmallAllocationIsUsable) {
    void* mem = sta_memory_pool::alloc(24);
    expect_usable_memory(mem, 24);
    sta_memory_pool::free(mem, 24);
}

TEST(StaMemoryPoolTest, MediumAllocationIsUsable) {
    void* mem = sta_memory_pool::alloc(240);
    expect_usable_memory(mem, 240);
    sta_memory_pool::free(mem, 240);
}

TEST(StaMemoryPoolTest, LargeAllocationIsUsable) {
    void* mem = sta_memory_pool::alloc(2400);
    expect_usable_memory(mem, 2400);
    sta_memory_pool::free(mem, 2400);
}

TEST(StaMemoryPoolTest, DifferentSizeClassesDoNotOverlap) {
    void* small = sta_memory_pool::alloc(24);
    void* large = sta_memory_pool::alloc(2400);

    expect_independent_allocations(small, large, 24);

    sta_memory_pool::free(small, 24);
    sta_memory_pool::free(large, 2400);
}

TEST(StaMemoryPoolTest, FreeListIsReusedAcrossRepeatedAllocations) {
    // The first allocation of a size class comes off the page cursor and
    // freeing it switches the class over to its free list, so the loop runs
    // through bump_alloc and free_list::get/put alike, without asserting on
    // the addresses handed back.
    for (int i = 0; i < 1000; ++i) {
        void* mem = sta_memory_pool::alloc(24);
        expect_usable_memory(mem, 24);
        sta_memory_pool::free(mem, 24);
    }
}

TEST(StaMemoryPoolTest, LargestPoolClassSizeIsUsable) {
    // A request of exactly MaxBlockSize still goes through the dispatch
    // tables rather than the heap, so the slot of the top class has to be
    // filled.
    void* mem = sta_memory_pool::alloc(sta_memory_pool::MaxBlockSize);
    expect_usable_memory(mem, sta_memory_pool::MaxBlockSize);
    sta_memory_pool::free(mem, sta_memory_pool::MaxBlockSize);
}

TEST(StaMemoryPoolTest, AboveMaxBlockSizeFallsBackToTheRawHeap) {
    constexpr size_t size = sta_memory_pool::MaxBlockSize + 1;
    void* mem = sta_memory_pool::alloc(size);
    expect_usable_memory(mem, size);
    sta_memory_pool::free(mem, size);
}

TEST(StaMemoryPoolTest, BlockSizeIsTheWholeClass) {
    EXPECT_EQ(sta_memory_pool::block_size(1), sta_memory_pool::MinBlockSize);
    EXPECT_EQ(sta_memory_pool::block_size(sizeof(void*)), sta_memory_pool::MinBlockSize);
    EXPECT_EQ(sta_memory_pool::block_size(sta_memory_pool::MinBlockSize),
              sta_memory_pool::MinBlockSize);
    EXPECT_EQ(sta_memory_pool::block_size(sta_memory_pool::MinBlockSize + 1),
              2 * sta_memory_pool::MinBlockSize);
    EXPECT_EQ(sta_memory_pool::block_size(100), 128u);
    EXPECT_EQ(sta_memory_pool::block_size(128), 128u);
    EXPECT_EQ(sta_memory_pool::block_size(129), 256u);
    EXPECT_EQ(sta_memory_pool::block_size(sta_memory_pool::MaxBlockSize),
              sta_memory_pool::MaxBlockSize);
}

/// A coroutine frame, like anything from operator new, is laid out for the default new alignment:
/// every block of every class is aligned to it, across the page boundary too.
TEST(StaMemoryPoolTest, EveryBlockIsAlignedForNew) {
    constexpr std::uintptr_t alignment = __STDCPP_DEFAULT_NEW_ALIGNMENT__;
    static_assert(sta_memory_pool::MinBlockSize % alignment == 0);

    // Enough of the smallest class to run through a whole page and into the next one.
    constexpr size_t count = sta_memory_pool::PageSize / sta_memory_pool::MinBlockSize + 16;
    std::vector<void*> blocks;
    blocks.reserve(count);

    for (size_t i = 0; i != count; ++i) {
        void* const block = sta_memory_pool::alloc(1);
        ASSERT_EQ(reinterpret_cast<std::uintptr_t>(block) % alignment, 0u) << "block " << i;
        blocks.push_back(block);
    }

    for (void* block : blocks) sta_memory_pool::free(block, 1);

    // Every class, from a request below the smallest block to the largest one, through the
    // free lists just filled and through the bump cursor.
    for (std::uint32_t size = 1; size <= sta_memory_pool::MaxBlockSize; size = size * 2 + 1) {
        void* const a = sta_memory_pool::alloc(size);
        void* const b = sta_memory_pool::alloc(size);
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(a) % alignment, 0u) << "size " << size;
        EXPECT_EQ(reinterpret_cast<std::uintptr_t>(b) % alignment, 0u) << "size " << size;
        sta_memory_pool::free(b, size);
        sta_memory_pool::free(a, size);
    }
}

/// A request below the smallest block is served from that class's list, like a request of the
/// block's own size: what one frees, the other gets back.
TEST(StaMemoryPoolTest, RequestsBelowTheSmallestBlockShareItsList) {
    void* const block = sta_memory_pool::alloc(sta_memory_pool::MinBlockSize);
    sta_memory_pool::free(block, sta_memory_pool::MinBlockSize);

    void* const tiny = sta_memory_pool::alloc(1);
    EXPECT_EQ(tiny, block);
    sta_memory_pool::free(tiny, 1);

    void* const again = sta_memory_pool::alloc(sta_memory_pool::MinBlockSize);
    EXPECT_EQ(again, block);
    sta_memory_pool::free(again, sta_memory_pool::MinBlockSize);
}

struct Small {
    std::byte data[24];
};

struct Medium {
    std::byte data[240];
};

TEST(StaAllocatorTest, VectorGrowsAndKeepsCorrectValues) {
    std::vector<int, sta_allocator<int>> v;

    for (int i = 0; i < 5000; ++i) v.push_back(i);

    ASSERT_EQ(v.size(), 5000u);
    for (int i = 0; i < 5000; ++i) EXPECT_EQ(v[i], i);
}

TEST(StaAllocatorTest, WorksForDifferentlySizedElements) {
    std::vector<Small, sta_allocator<Small>> small_objects(100);
    std::vector<Medium, sta_allocator<Medium>> medium_objects(100);

    for (auto& obj : small_objects) obj.data[0] = std::byte{0xAB};
    for (auto& obj : medium_objects) obj.data[0] = std::byte{0xCD};

    for (auto& obj : small_objects) EXPECT_EQ(obj.data[0], std::byte{0xAB});
    for (auto& obj : medium_objects) EXPECT_EQ(obj.data[0], std::byte{0xCD});
}

}  // namespace
