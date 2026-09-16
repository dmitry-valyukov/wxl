#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;



using namespace wxl::core;
using namespace wxl::async;

TEST(BumpBufferTest, AllocReturnsDistinctWordAlignedBlocksUntilExhausted) {
    constexpr size_t buff_size = sizeof(size_t) * 2;
    bump_buffer<buff_size> buf;

    char* data1 = static_cast<char*>(buf.alloc(1));
    ASSERT_NE(data1, nullptr);
    EXPECT_TRUE(buf.contains(data1));

    char* data2 = static_cast<char*>(buf.alloc(1));
    ASSERT_NE(data2, nullptr);
    EXPECT_TRUE(buf.contains(data2));

    EXPECT_EQ(data2 - data1, static_cast<ptrdiff_t>(sizeof(size_t)));

    // buffer holds exactly two words; a third allocation must fail
    char* data3 = static_cast<char*>(buf.alloc(1));
    EXPECT_EQ(data3, nullptr);
    EXPECT_FALSE(buf.contains(data3));
}

TEST(BumpBufferTest, AllocRoundsSizeUpToAWholeWord) {
    constexpr size_t buff_size = sizeof(size_t) * 4;
    bump_buffer<buff_size> buf;

    // a 1-byte request still consumes a whole word, so the next allocation
    // starts exactly one word later
    char* data1 = static_cast<char*>(buf.alloc(1));
    char* data2 = static_cast<char*>(buf.alloc(sizeof(size_t)));
    ASSERT_NE(data1, nullptr);
    ASSERT_NE(data2, nullptr);
    EXPECT_EQ(data2 - data1, static_cast<ptrdiff_t>(sizeof(size_t)));
}

TEST(BumpBufferTest, AddressesOutsideTheBufferAreNotContained) {
    bump_buffer<sizeof(size_t)> buf;
    int unrelated;

    EXPECT_FALSE(buf.contains(&unrelated));
    EXPECT_FALSE(buf.contains(nullptr));
}

// Regression test: alloc()'s CAS retry loop must re-read the cursor after a
// failed compare_exchange instead of retrying with the stale value (which
// would either spin forever or, moved to a different design, corrupt
// bookkeeping). cells threads race for exactly `cells` word-sized slots, each
// attempting two allocations; exactly `cells` must succeed and `cells` must
// see the buffer already exhausted.
TEST(BumpBufferTest, ConcurrentAllocRaceGrantsExactlyOneSlotPerThread) {
    constexpr size_t cells = 32;
    using test_buffer = bump_buffer<sizeof(size_t) * cells>;

    test_buffer buf;
    std::atomic<size_t> allocated{};
    std::atomic<size_t> failed{};
    std::barrier sync_point(static_cast<ptrdiff_t>(cells));

    auto try_allocate = [&] {
        sync_point.arrive_and_wait();

        for (int i = 0; i < 2; ++i) {
            void* data = buf.alloc(1);

            if (buf.contains(data))
                allocated.fetch_add(1);
            else
                failed.fetch_add(1);
        }
    };

    std::vector<std::thread> threads;
    threads.reserve(cells);
    for (size_t i = 0; i < cells; ++i)
        threads.emplace_back(try_allocate);

    for (auto& t : threads) t.join();

    EXPECT_EQ(allocated.load(), cells);
    EXPECT_EQ(failed.load(), cells);
}
