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
    // The first allocation of a size class hits the page bump-allocator;
    // freeing it switches that class over to the free-list, so looping
    // exercises both alloc_<N> and pool_<N>::get/put without asserting on
    // the exact addresses handed back (an implementation detail).
    for (int i = 0; i < 1000; ++i) {
        void* mem = sta_memory_pool::alloc(24);
        expect_usable_memory(mem, 24);
        sta_memory_pool::free(mem, 24);
    }
}

TEST(StaMemoryPoolTest, LargestPoolClassSizeIsUsable) {
    // Regression test: requests right up to MaxBlockSize still route
    // through the pool dispatch tables (s_alloc/s_free) rather than the
    // raw-heap fallback, and that top size class's table slot must be
    // populated.
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
    EXPECT_EQ(sta_memory_pool::block_size(1), sizeof(void*));
    EXPECT_EQ(sta_memory_pool::block_size(sizeof(void*)), sizeof(void*));
    EXPECT_EQ(sta_memory_pool::block_size(sizeof(void*) + 1), 2 * sizeof(void*));
    EXPECT_EQ(sta_memory_pool::block_size(100), 128u);
    EXPECT_EQ(sta_memory_pool::block_size(128), 128u);
    EXPECT_EQ(sta_memory_pool::block_size(129), 256u);
    EXPECT_EQ(sta_memory_pool::block_size(sta_memory_pool::MaxBlockSize),
              sta_memory_pool::MaxBlockSize);
}

TEST(StaMemoryPoolTest, GrowingWithinTheClassIsFree) {
    void* mem = sta_memory_pool::alloc(100);  // a 128-byte block
    std::memset(mem, 0xCD, 100);

    EXPECT_TRUE(sta_memory_pool::try_extend(mem, 100, 128));
    EXPECT_EQ(static_cast<unsigned char*>(mem)[99], 0xCD);

    sta_memory_pool::free(mem, 128);
}

TEST(StaMemoryPoolTest, TheBlockOnTopGrowsWhereItStands) {
    // 16k is a size class no other test here touches, so its free list is
    // empty and this block can only have come from the bump cursor -- which
    // makes it the one on top.
    constexpr uint32_t size = 16 * 1024;

    void* mem = sta_memory_pool::alloc(size);
    std::memset(mem, 0xEF, size);

    ASSERT_TRUE(sta_memory_pool::try_extend(mem, size, 2 * size));

    EXPECT_EQ(static_cast<unsigned char*>(mem)[0], 0xEF) << "the block was moved";
    EXPECT_EQ(static_cast<unsigned char*>(mem)[size - 1], 0xEF) << "the block was moved";

    // The room past the old end belongs to the block now.
    expect_usable_memory(mem, 2 * size);

    sta_memory_pool::free(mem, 2 * size);
}

TEST(StaMemoryPoolTest, ABlockWithANeighbourBehindItDoesNotGrow) {
    constexpr uint32_t size = 8 * 1024;

    void* first = sta_memory_pool::alloc(size);
    void* second = sta_memory_pool::alloc(size);

    EXPECT_FALSE(sta_memory_pool::try_extend(first, size, 2 * size));

    expect_independent_allocations(first, second, size);

    sta_memory_pool::free(second, size);
    sta_memory_pool::free(first, size);
}

TEST(StaMemoryPoolTest, GrowingPastThePoolCeilingIsRefused) {
    constexpr uint32_t size = sta_memory_pool::MaxBlockSize;

    void* mem = sta_memory_pool::alloc(size);

    EXPECT_FALSE(sta_memory_pool::try_extend(mem, size, size + 1));

    sta_memory_pool::free(mem, size);
}

struct Small {
    std::byte data[24];
};

struct Medium {
    std::byte data[240];
};

TEST(StaAllocatorTest, VectorGrowsAndKeepsCorrectValues) {
    std::vector<int, sta_allocator<int>> v;

    for (int i = 0; i < 5000; ++i)
        v.push_back(i);

    ASSERT_EQ(v.size(), 5000u);
    for (int i = 0; i < 5000; ++i)
        EXPECT_EQ(v[i], i);
}

TEST(StaAllocatorTest, WorksForDifferentlySizedElements) {
    std::vector<Small, sta_allocator<Small>> small_objects(100);
    std::vector<Medium, sta_allocator<Medium>> medium_objects(100);

    for (auto& obj : small_objects)
        obj.data[0] = std::byte{0xAB};
    for (auto& obj : medium_objects)
        obj.data[0] = std::byte{0xCD};

    for (auto& obj : small_objects)
        EXPECT_EQ(obj.data[0], std::byte{0xAB});
    for (auto& obj : medium_objects)
        EXPECT_EQ(obj.data[0], std::byte{0xCD});
}

}  // namespace
