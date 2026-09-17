#include <gtest/gtest.h>

import std;
import wxl.core;

namespace {

using wxl::core::sta_allocator;

// The standard containers with the pool's allocator and nothing more, so a
// function written for std::vector<T, sta_allocator<T>> takes these as they are.
static_assert(std::same_as<wxl::core::sta_vector<int>, std::vector<int, sta_allocator<int>>>);
static_assert(std::same_as<wxl::core::sta_deque<int>, std::deque<int, sta_allocator<int>>>);
static_assert(std::same_as<wxl::core::sta_wstring,
                           std::basic_string<wchar_t, std::char_traits<wchar_t>, sta_allocator<wchar_t>>>);

TEST(StaStlTest, VectorGrowsPastTheLargestPoolBlock) {
    // Ten thousand ints outgrow MaxBlockSize on the way, so the buffer moves
    // from the pool's size classes to the ordinary allocator and keeps what it
    // held.
    wxl::core::sta_vector<int> numbers;
    for (int i = 0; i < 10000; ++i)
        numbers.push_back(i);

    ASSERT_EQ(numbers.size(), 10000u);
    EXPECT_GT(numbers.capacity() * sizeof(int), wxl::core::sta_memory_pool::MaxBlockSize);
    for (int i = 0; i < 10000; ++i)
        ASSERT_EQ(numbers[static_cast<std::size_t>(i)], i);
}

TEST(StaStlTest, DequeDoesNotMoveWhatItHolds) {
    wxl::core::sta_deque<int> numbers;
    numbers.push_back(42);
    const int* first = &numbers.front();

    for (int i = 0; i < 10000; ++i)
        numbers.push_back(i);

    EXPECT_EQ(&numbers.front(), first);
    EXPECT_EQ(*first, 42);
}

}  // namespace
