#include <gtest/gtest.h>

import std;
import wxl.core;

namespace {

using namespace wxl::core;

template <typename Key, typename T>
using sta_pair_allocator = sta_allocator<std::pair<const Key, T>>;

// The standard containers with the pool's allocator and nothing more, so a
// function written for std::vector<T, sta_allocator<T>> takes these as they are.
static_assert(std::same_as<sta_vector<int>, std::vector<int, sta_allocator<int>>>);
static_assert(std::same_as<sta_deque<int>, std::deque<int, sta_allocator<int>>>);
static_assert(
    std::same_as<sta_wstring,
                 std::basic_string<wchar_t, std::char_traits<wchar_t>, sta_allocator<wchar_t>>>);
static_assert(
    std::same_as<sta_u8string,
                 std::basic_string<char8_t, std::char_traits<char8_t>, sta_allocator<char8_t>>>);
static_assert(
    std::same_as<sta_u16string,
                 std::basic_string<char16_t, std::char_traits<char16_t>, sta_allocator<char16_t>>>);
static_assert(
    std::same_as<sta_u32string,
                 std::basic_string<char32_t, std::char_traits<char32_t>, sta_allocator<char32_t>>>);
static_assert(std::same_as<sta_list<int>, std::list<int, sta_allocator<int>>>);
static_assert(std::same_as<sta_forward_list<int>, std::forward_list<int, sta_allocator<int>>>);
static_assert(std::same_as<sta_set<int>, std::set<int, std::less<int>, sta_allocator<int>>>);
static_assert(
    std::same_as<sta_multiset<int>, std::multiset<int, std::less<int>, sta_allocator<int>>>);
static_assert(std::same_as<sta_map<int, char>,
                           std::map<int, char, std::less<int>, sta_pair_allocator<int, char>>>);
static_assert(
    std::same_as<sta_multimap<int, char>,
                 std::multimap<int, char, std::less<int>, sta_pair_allocator<int, char>>>);
static_assert(
    std::same_as<sta_unordered_set<int>,
                 std::unordered_set<int, std::hash<int>, std::equal_to<int>, sta_allocator<int>>>);
static_assert(std::same_as<sta_unordered_multiset<int>,
                           std::unordered_multiset<int, std::hash<int>, std::equal_to<int>,
                                                   sta_allocator<int>>>);
static_assert(std::same_as<sta_unordered_map<int, char>,
                           std::unordered_map<int, char, std::hash<int>, std::equal_to<int>,
                                              sta_pair_allocator<int, char>>>);
static_assert(std::same_as<sta_unordered_multimap<int, char>,
                           std::unordered_multimap<int, char, std::hash<int>, std::equal_to<int>,
                                                   sta_pair_allocator<int, char>>>);
static_assert(std::same_as<sta_flat_set<int>, std::flat_set<int, std::less<int>, sta_vector<int>>>);
static_assert(
    std::same_as<sta_flat_multiset<int>, std::flat_multiset<int, std::less<int>, sta_vector<int>>>);
static_assert(
    std::same_as<sta_flat_map<int, char>,
                 std::flat_map<int, char, std::less<int>, sta_vector<int>, sta_vector<char>>>);
static_assert(
    std::same_as<sta_flat_multimap<int, char>,
                 std::flat_multimap<int, char, std::less<int>, sta_vector<int>, sta_vector<char>>>);
static_assert(std::same_as<sta_stack<int>, std::stack<int, sta_deque<int>>>);
static_assert(std::same_as<sta_queue<int>, std::queue<int, sta_deque<int>>>);
static_assert(std::same_as<sta_priority_queue<int>,
                           std::priority_queue<int, sta_vector<int>, std::less<int>>>);

constexpr int Count = 1000;

const auto evens = std::views::iota(0, Count) | std::views::stride(2);
const auto odd = [](int i) { return i % 2 != 0; };
const auto odd_key = [](const auto& entry) { return entry.first % 2 != 0; };

TEST(StaStlTest, VectorGrowsPastTheLargestPoolBlock) {
    // Ten thousand ints outgrow MaxBlockSize on the way, so the buffer moves
    // from the pool's size classes to the ordinary allocator and keeps what it
    // held.
    sta_vector<int> numbers;
    for (int i = 0; i < 10000; ++i)
        numbers.push_back(i);

    ASSERT_EQ(numbers.size(), 10000u);
    EXPECT_GT(numbers.capacity() * sizeof(int), sta_memory_pool::MaxBlockSize);
    for (int i = 0; i < 10000; ++i)
        ASSERT_EQ(numbers[static_cast<std::size_t>(i)], i);
}

TEST(StaStlTest, DequeDoesNotMoveWhatItHolds) {
    sta_deque<int> numbers;
    numbers.push_back(42);
    const int* first = &numbers.front();

    for (int i = 0; i < 10000; ++i)
        numbers.push_back(i);

    EXPECT_EQ(&numbers.front(), first);
    EXPECT_EQ(*first, 42);
}

TEST(StaStlTest, ListsSortAndErase) {
    sta_list<int> list;
    sta_forward_list<int> forward;
    for (int i = 0; i < Count; ++i) {
        list.push_front(i);
        forward.push_front(i);
    }

    list.sort();
    forward.sort();
    std::erase_if(list, odd);
    std::erase_if(forward, odd);

    const sta_list<int> copy = list;
    EXPECT_TRUE(std::ranges::equal(copy, evens));
    EXPECT_TRUE(std::ranges::equal(forward, evens));
}

TEST(StaStlTest, OrderedContainersKeepTheirOrder) {
    sta_set<int> set;
    sta_multiset<int> multiset;
    sta_map<int, int> map;
    sta_multimap<int, int> multimap;
    sta_flat_set<int> flat_set;
    sta_flat_multiset<int> flat_multiset;
    sta_flat_map<int, int> flat_map;
    sta_flat_multimap<int, int> flat_multimap;

    for (int i = Count; i-- > 0;) {
        set.insert(i);
        multiset.insert(i);
        map.emplace(i, i);
        multimap.emplace(i, i);
        flat_set.insert(i);
        flat_multiset.insert(i);
        flat_map.emplace(i, i);
        flat_multimap.emplace(i, i);
    }

    std::erase_if(set, odd);
    std::erase_if(multiset, odd);
    std::erase_if(map, odd_key);
    std::erase_if(multimap, odd_key);
    std::erase_if(flat_set, odd);
    std::erase_if(flat_multiset, odd);
    std::erase_if(flat_map, odd_key);
    std::erase_if(flat_multimap, odd_key);

    EXPECT_TRUE(std::ranges::equal(set, evens));
    EXPECT_TRUE(std::ranges::equal(multiset, evens));
    EXPECT_TRUE(std::ranges::equal(map | std::views::keys, evens));
    EXPECT_TRUE(std::ranges::equal(multimap | std::views::keys, evens));
    EXPECT_TRUE(std::ranges::equal(flat_set, evens));
    EXPECT_TRUE(std::ranges::equal(flat_multiset, evens));
    EXPECT_TRUE(std::ranges::equal(flat_map.keys(), evens));
    EXPECT_TRUE(std::ranges::equal(flat_multimap.values(), evens));
}

TEST(StaStlTest, HashBucketsGrowPastTheLargestPoolBlock) {
    // Ten thousand keys take more buckets than a pool block has room for, so
    // the bucket array leaves the pool while the nodes stay in it.
    sta_unordered_map<int, int> squares;
    for (int i = 0; i < 10000; ++i)
        squares.emplace(i, i * i);

    EXPECT_GT(squares.bucket_count() * sizeof(void*), sta_memory_pool::MaxBlockSize);
    for (int i = 0; i < 10000; ++i)
        ASSERT_EQ(squares.at(i), i * i);

    sta_unordered_set<int> set(evens.begin(), evens.end());
    sta_unordered_multiset<int> multiset{1, 1, 2};
    sta_unordered_multimap<int, int> multimap{{1, 1}, {1, 2}, {2, 2}};

    EXPECT_EQ(set.size(), static_cast<std::size_t>(Count / 2));
    EXPECT_TRUE(set.contains(Count - 2));
    EXPECT_FALSE(set.contains(Count - 1));
    EXPECT_EQ(multiset.count(1), 2u);
    EXPECT_EQ(multimap.count(1), 2u);
}

TEST(StaStlTest, AdaptorsKeepTheirOrderOfRemoval) {
    sta_stack<int> stack;
    sta_queue<int> queue;
    sta_priority_queue<int> heap;
    for (int i : {2, 3, 1}) {
        stack.push(i);
        queue.push(i);
        heap.push(i);
    }

    EXPECT_EQ(stack.top(), 1);
    EXPECT_EQ(queue.front(), 2);
    EXPECT_EQ(heap.top(), 3);
}

}  // namespace
