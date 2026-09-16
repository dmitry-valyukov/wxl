#include <gtest/gtest.h>

import std;
import wxl.core;
import wxl.async;

using namespace wxl::async;

namespace {

/// Counts its own live instances, so a test can state that a queue destroyed with
/// elements still in it destroys exactly those elements and no others.
struct counted {
    static inline int alive = 0;

    int value;

    explicit counted(int v = 0) : value(v) { ++alive; }
    counted(const counted& other) : value(other.value) { ++alive; }
    counted(counted&& other) noexcept : value(other.value) { ++alive; }

    counted& operator=(const counted&) = default;
    counted& operator=(counted&&) noexcept = default;

    ~counted() { --alive; }
};

/// The two queues answer the same question -- which block does the writer fill next -- the
/// same way, and differ in how the block gets from the reader to the writer, so every test
/// below is written once and run against both. `spsc_queue` hands blocks over through a slot
/// with one writer per word; `spsc_queue_reference_implementation` does it with a CAS and an
/// exchange, and is kept to measure the first against.
template <template <class, size_t> class queue_tt>
struct flavor {
    template <class element_t, size_t block_size>
    using queue = queue_tt<element_t, block_size>;
};

/// What a flavour is called in the name of every test it runs. It hangs off the type and
/// not off its place in the list below, because gtest hands its name generator an index:
/// read the label off that, and the day somebody reorders `testing::Types` the suite called
/// `reference` quietly starts running a different queue, with nothing to fail.
template <class flavor_t>
struct flavor_label;

template <>
struct flavor_label<flavor<spsc_queue>> {
    static constexpr const char* value = "queue";
};

template <>
struct flavor_label<flavor<spsc_queue_reference_implementation>> {
    static constexpr const char* value = "reference";
};

class flavor_name
{
public:
    template <class flavor_t>
    static std::string GetName(int) {
        return flavor_label<flavor_t>::value;
    }
};

template <class flavor_t>
class SpscQueueTest : public testing::Test
{
};

using flavors = testing::Types<flavor<spsc_queue>, flavor<spsc_queue_reference_implementation>>;

}  // namespace

TYPED_TEST_SUITE(SpscQueueTest, flavors, flavor_name);

TYPED_TEST(SpscQueueTest, ReadFindsNothingInAnEmptyQueue) {
    using queue_t = typename TypeParam::template queue<int, 4>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    int value = -1;
    EXPECT_FALSE(reader.read(value));
    EXPECT_EQ(value, -1);
}

TYPED_TEST(SpscQueueTest, ReadsBackWhatWasWrittenInOrder) {
    using queue_t = typename TypeParam::template queue<int, 4>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    for (int i = 0; i < 3; ++i) queue.write(i);

    for (int expected = 0; expected < 3; ++expected) {
        int value = -1;
        ASSERT_TRUE(reader.read(value));
        EXPECT_EQ(value, expected);
    }

    int value = -1;
    EXPECT_FALSE(reader.read(value));
}

TYPED_TEST(SpscQueueTest, WritesAndReadsAcrossManyBlocks) {
    using queue_t = typename TypeParam::template queue<int, 4>;

    constexpr int total = 4 * 25 + 3;  // several whole blocks and a part of one

    queue_t queue;
    typename queue_t::reader reader(queue);

    for (int i = 0; i < total; ++i) queue.write(i);

    for (int expected = 0; expected < total; ++expected) {
        int value = -1;
        ASSERT_TRUE(reader.read(value)) << "at " << expected;
        EXPECT_EQ(value, expected);
    }

    int value = -1;
    EXPECT_FALSE(reader.read(value));
}

TYPED_TEST(SpscQueueTest, InterleavedWritingAndReadingKeepsOrder) {
    using queue_t = typename TypeParam::template queue<int, 4>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    int next_to_read = 0;

    for (int i = 0; i < 100; ++i) {
        queue.write(i);

        if (i % 3 == 0) {
            int value = -1;
            ASSERT_TRUE(reader.read(value));
            EXPECT_EQ(value, next_to_read++);
        }
    }

    for (int value = -1; reader.read(value); value = -1) EXPECT_EQ(value, next_to_read++);

    EXPECT_EQ(next_to_read, 100);
}

TYPED_TEST(SpscQueueTest, DrainedBlocksAreReusedInsteadOfAllocated) {
    // Written and read one block at a time, the queue should settle on the blocks it
    // already has: with a peak of one full block outstanding there is nothing to grow for.
    // Reuse is not observable from outside, so the sanity of it is that nothing leaks and
    // everything comes back in order over far more elements than any working set.
    using queue_t = typename TypeParam::template queue<int, 4>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    for (int round = 0; round < 1000; ++round) {
        for (int i = 0; i < 4; ++i) queue.write(round * 4 + i);

        for (int i = 0; i < 4; ++i) {
            int value = -1;
            ASSERT_TRUE(reader.read(value));
            EXPECT_EQ(value, round * 4 + i);
        }
    }
}

TYPED_TEST(SpscQueueTest, EmplaceBuildsTheElementInPlace) {
    using queue_t = typename TypeParam::template queue<std::string, 2>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    queue.emplace(5, 'x');
    queue.write(std::string("moved"));

    std::string value;
    ASSERT_TRUE(reader.read(value));
    EXPECT_EQ(value, "xxxxx");
    ASSERT_TRUE(reader.read(value));
    EXPECT_EQ(value, "moved");
}

TYPED_TEST(SpscQueueTest, ReadingAnElementDestroysItsSlot) {
    using queue_t = typename TypeParam::template queue<counted, 2>;

    counted::alive = 0;

    {
        queue_t queue;
        typename queue_t::reader reader(queue);

        for (int i = 0; i < 6; ++i) queue.emplace(i);
        EXPECT_EQ(counted::alive, 6);

        counted taken;
        for (int i = 0; i < 6; ++i) ASSERT_TRUE(reader.read(taken));

        EXPECT_EQ(counted::alive, 1);  // only the one moved out into `taken`
    }

    EXPECT_EQ(counted::alive, 0);
}

TYPED_TEST(SpscQueueTest, DestroyingTheQueueDestroysWhatWasNeverRead) {
    using queue_t = typename TypeParam::template queue<counted, 2>;

    counted::alive = 0;

    {
        queue_t queue;
        typename queue_t::reader reader(queue);

        for (int i = 0; i < 7; ++i) queue.emplace(i);

        counted taken;
        ASSERT_TRUE(reader.read(taken));
        EXPECT_EQ(counted::alive, 7);  // six left in the queue, one moved out
    }

    EXPECT_EQ(counted::alive, 0);
}

TYPED_TEST(SpscQueueTest, AQueueThatNeverHadAReaderStillFreesItsElements) {
    using queue_t = typename TypeParam::template queue<counted, 2>;

    counted::alive = 0;

    {
        queue_t queue;

        for (int i = 0; i < 5; ++i) queue.emplace(i);
        EXPECT_EQ(counted::alive, 5);
    }

    EXPECT_EQ(counted::alive, 0);
}

TYPED_TEST(SpscQueueTest, CarriesEverythingBetweenTwoThreads) {
    using queue_t = typename TypeParam::template queue<int, 64>;

    constexpr int total = 200'000;

    queue_t queue;
    typename queue_t::reader reader(queue);

    std::atomic<bool> writing = true;

    std::thread producer([&] {
        for (int i = 0; i < total; ++i) queue.write(i);
        writing.store(false, std::memory_order_release);
    });

    int next_expected = 0;
    bool producer_done = false;

    // Not a wait: the consumer runs the queue dry as fast as it can and stops once the
    // producer is done. A false read only says the queue was empty at that moment; once the
    // producer's flag reads false everything it wrote is visible, so the next empty read is
    // the end of the data rather than a race with it. Where a consumer has to sleep instead
    // of spinning, it sleeps on an event of its own -- the queue signals nobody.
    while (next_expected < total) {
        int value = -1;

        if (reader.read(value)) {
            ASSERT_EQ(value, next_expected) << "at " << next_expected;
            ++next_expected;
            continue;
        }

        if (producer_done) break;

        producer_done = !writing.load(std::memory_order_acquire);
    }

    producer.join();

    EXPECT_EQ(next_expected, total);

    int leftover = -1;
    EXPECT_FALSE(reader.read(leftover));
}

TYPED_TEST(SpscQueueTest, GrowsForABurstAndKeepsReusingWhatItGrew) {
    // Two elements to a block, so a burst of twenty grows ten blocks and the reader then
    // hands every one of them back. Three bursts in a row: if a block were ever taken while
    // the reader still had it, or lost on the way back, the elements would stop matching --
    // and the queue's own teardown assert says nothing leaked.
    using queue_t = typename TypeParam::template queue<counted, 2>;

    counted::alive = 0;

    {
        queue_t queue;
        typename queue_t::reader reader(queue);

        for (int burst = 0; burst < 3; ++burst) {
            for (int i = 0; i < 20; ++i) queue.emplace(i);
            EXPECT_EQ(counted::alive, 20);

            counted taken;
            for (int i = 0; i < 20; ++i) {
                ASSERT_TRUE(reader.read(taken));
                EXPECT_EQ(taken.value, i);
            }

            EXPECT_EQ(counted::alive, 1);  // only the one moved out into `taken`
        }
    }

    EXPECT_EQ(counted::alive, 0);
}

TYPED_TEST(SpscQueueTest, WorksWithTheSmallestPossibleGeometry) {
    // One element per block: every write retires a block and every read frees one, so the
    // two sides hand blocks over on every single element.
    using queue_t = typename TypeParam::template queue<int, 1>;

    queue_t queue;
    typename queue_t::reader reader(queue);

    for (int i = 0; i < 50; ++i) {
        queue.write(i);

        int value = -1;
        ASSERT_TRUE(reader.read(value));
        EXPECT_EQ(value, i);
    }

    int value = -1;
    EXPECT_FALSE(reader.read(value));
}
