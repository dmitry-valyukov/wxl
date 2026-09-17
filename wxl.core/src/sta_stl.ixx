// Standard containers whose memory comes from the STA pool.
//
// Spelled out here, once, because the alternative is every module writing the
// same line: the type is the same one whoever declares it, and two spellings of
// it would only be two names for one thing.
//
// Every one of them is as thread-bound as the pool is: made, grown and
// destroyed on its thread, and nowhere else.

export module wxl.core:sta_stl;

import :sta_allocator;
import std;

export namespace wxl::core {

/// Strings whose characters live in the STA pool.
///@{
using sta_string = std::basic_string<char, std::char_traits<char>, sta_allocator<char>>;

using sta_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, sta_allocator<wchar_t>>;

using sta_u8string = std::basic_string<char8_t, std::char_traits<char8_t>, sta_allocator<char8_t>>;

using sta_u16string =
    std::basic_string<char16_t, std::char_traits<char16_t>, sta_allocator<char16_t>>;

using sta_u32string =
    std::basic_string<char32_t, std::char_traits<char32_t>, sta_allocator<char32_t>>;
///@}

/// A vector whose buffer lives in the STA pool.
template <typename T>
using sta_vector = std::vector<T, sta_allocator<T>>;

/// A deque from the same pool. It keeps what it holds where it was put:
/// growing it moves nothing, so a pointer to an element lives as long as the
/// element does.
template <typename T>
using sta_deque = std::deque<T, sta_allocator<T>>;

/// Lists whose nodes live in the STA pool.
///@{
template <typename T>
using sta_list = std::list<T, sta_allocator<T>>;

template <typename T>
using sta_forward_list = std::forward_list<T, sta_allocator<T>>;
///@}

/// Ordered sets and maps whose nodes live in the STA pool.
///@{
template <typename Key, typename Compare = std::less<Key>>
using sta_set = std::set<Key, Compare, sta_allocator<Key>>;

template <typename Key, typename Compare = std::less<Key>>
using sta_multiset = std::multiset<Key, Compare, sta_allocator<Key>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using sta_map = std::map<Key, T, Compare, sta_allocator<std::pair<const Key, T>>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using sta_multimap = std::multimap<Key, T, Compare, sta_allocator<std::pair<const Key, T>>>;
///@}

/// Hash sets and maps whose nodes and buckets live in the STA pool.
///@{
template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using sta_unordered_set = std::unordered_set<Key, Hash, KeyEqual, sta_allocator<Key>>;

template <typename Key, typename Hash = std::hash<Key>, typename KeyEqual = std::equal_to<Key>>
using sta_unordered_multiset = std::unordered_multiset<Key, Hash, KeyEqual, sta_allocator<Key>>;

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
using sta_unordered_map =
    std::unordered_map<Key, T, Hash, KeyEqual, sta_allocator<std::pair<const Key, T>>>;

template <typename Key, typename T, typename Hash = std::hash<Key>,
          typename KeyEqual = std::equal_to<Key>>
using sta_unordered_multimap =
    std::unordered_multimap<Key, T, Hash, KeyEqual, sta_allocator<std::pair<const Key, T>>>;
///@}

/// Sorted sets and maps kept in sta_vector.
///@{
template <typename Key, typename Compare = std::less<Key>>
using sta_flat_set = std::flat_set<Key, Compare, sta_vector<Key>>;

template <typename Key, typename Compare = std::less<Key>>
using sta_flat_multiset = std::flat_multiset<Key, Compare, sta_vector<Key>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using sta_flat_map = std::flat_map<Key, T, Compare, sta_vector<Key>, sta_vector<T>>;

template <typename Key, typename T, typename Compare = std::less<Key>>
using sta_flat_multimap = std::flat_multimap<Key, T, Compare, sta_vector<Key>, sta_vector<T>>;
///@}

/// Adaptors over the pooled containers, each on the one std puts under it by default.
///@{
template <typename T>
using sta_stack = std::stack<T, sta_deque<T>>;

template <typename T>
using sta_queue = std::queue<T, sta_deque<T>>;

template <typename T, typename Compare = std::less<T>>
using sta_priority_queue = std::priority_queue<T, sta_vector<T>, Compare>;
///@}

}  // export namespace wxl::core
