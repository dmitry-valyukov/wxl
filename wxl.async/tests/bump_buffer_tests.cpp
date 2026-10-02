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

TEST(BumpBufferTest, RequestLargerThanTheBufferFailsWithoutConsumingAnything) {
    constexpr size_t buff_size = sizeof(size_t) * 4;
    bump_buffer<buff_size> buf;

    // Rounded up before the check, SIZE_MAX would turn into 0 and succeed.
    EXPECT_EQ(buf.alloc(std::numeric_limits<size_t>::max()), nullptr);
    EXPECT_EQ(buf.alloc(buff_size + 1), nullptr);

    // Neither request claimed anything: the whole buffer is still there in one piece.
    char* whole = static_cast<char*>(buf.alloc(buff_size));
    ASSERT_NE(whole, nullptr);
    EXPECT_TRUE(buf.contains(whole));
    EXPECT_TRUE(buf.contains(whole + buff_size - 1));
    EXPECT_EQ(buf.alloc(1), nullptr);
}

TEST(BumpBufferTest, RequestThatDoesNotFitLeavesTheTailForSmallerOnes) {
    constexpr size_t buff_size = sizeof(size_t) * 4;
    bump_buffer<buff_size> buf;

    char* head = static_cast<char*>(buf.alloc(sizeof(size_t) * 2));
    ASSERT_NE(head, nullptr);

    EXPECT_EQ(buf.alloc(sizeof(size_t) * 3), nullptr);

    char* tail = static_cast<char*>(buf.alloc(sizeof(size_t) * 2));
    ASSERT_NE(tail, nullptr);
    EXPECT_EQ(tail - head, static_cast<ptrdiff_t>(sizeof(size_t) * 2));
}

TEST(BumpBufferTest, AddressesOutsideTheBufferAreNotContained) {
    bump_buffer<sizeof(size_t)> buf;
    int unrelated;

    EXPECT_FALSE(buf.contains(&unrelated));
    EXPECT_FALSE(buf.contains(nullptr));
}

// cells threads race for exactly `cells` word-sized slots, each attempting two
// allocations; exactly `cells` must succeed and `cells` must see the buffer
// already exhausted. With all sizes equal, the first claim to land beyond the end
// comes only after the buffer is full, so no slot is lost to the race.
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

// Threads race for blocks of different sizes at the edge of the buffer, so claims
// keep landing beyond the end and being undone. An undo that subtracted from the
// cursor as it is now, rather than only from the claim nobody bumped past, would
// let a later alloc() hand out a block that another thread already holds. Each
// round gives a fresh buffer to all threads at once, and each thread keeps asking
// until it has been refused several times; afterwards no two blocks granted from
// one buffer may overlap.
TEST(BumpBufferTest, ConcurrentAllocOfMixedSizesNeverHandsOutOverlappingBlocks) {
    constexpr size_t buff_size = sizeof(size_t) * 32;
    constexpr size_t rounds = 10000;
    constexpr size_t refusals_per_round = 8;
    constexpr std::array sizes{sizeof(size_t) * 1, sizeof(size_t) * 8,  sizeof(size_t) * 2,
                               sizeof(size_t) * 5, sizeof(size_t) * 1, sizeof(size_t) * 3,
                               sizeof(size_t) * 2, sizeof(size_t) * 12};
    using test_buffer = bump_buffer<buff_size>;

    struct block {
        size_t round;
        const char* begin;
        size_t size;
    };

    const size_t threads = std::clamp<size_t>(std::thread::hardware_concurrency(), 2, 8);
    auto buffers = std::make_unique<test_buffer[]>(rounds);
    std::vector<std::vector<block>> granted(threads);

    // A spin gate rather than std::barrier: a barrier wakes its waiters through the
    // kernel microseconds apart, and the first one awake fills the buffer alone. Past
    // a few thousand pauses the gate yields, in case a thread it waits for has lost
    // its core.
    std::atomic<size_t> arrived{0};
    auto start_together = [&](size_t round) {
        const size_t everyone = threads * (round + 1);
        arrived.fetch_add(1);

        for (size_t spins = 0; arrived.load() < everyone; ++spins) {
            if (spins < 4096)
                cpu_pause();
            else
                std::this_thread::yield();
        }
    };

    auto race = [&](size_t thread_index) {
        std::vector<block>& mine = granted[thread_index];
        mine.reserve(rounds * buff_size / sizeof(size_t) / threads * 2);

        for (size_t round = 0; round < rounds; ++round) {
            start_together(round);

            for (size_t i = thread_index, refusals = 0; refusals < refusals_per_round; ++i) {
                const size_t size = sizes[i % sizes.size()];

                if (void* data = buffers[round].alloc(size))
                    mine.push_back({round, static_cast<const char*>(data), size});
                else
                    ++refusals;
            }
        }
    };

    std::vector<std::thread> workers;
    workers.reserve(threads);
    for (size_t i = 0; i < threads; ++i)
        workers.emplace_back(race, i);

    for (auto& t : workers) t.join();

    std::vector<block> all;
    for (const auto& mine : granted)
        all.insert(all.end(), mine.begin(), mine.end());

    std::ranges::sort(all, [](const block& a, const block& b) {
        return a.round != b.round ? a.round < b.round : a.begin < b.begin;
    });

    size_t outside = 0;
    size_t overlaps = 0;
    for (size_t i = 0; i < all.size(); ++i) {
        const block& b = all[i];

        if (!buffers[b.round].contains(b.begin) || !buffers[b.round].contains(b.begin + b.size - 1))
            ++outside;

        if (i > 0 && all[i - 1].round == b.round && all[i - 1].begin + all[i - 1].size > b.begin)
            ++overlaps;
    }

    EXPECT_EQ(outside, 0u);
    EXPECT_EQ(overlaps, 0u);
}
