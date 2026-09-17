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
///@}

/// A vector whose buffer lives in the STA pool.
template <typename T>
using sta_vector = std::vector<T, sta_allocator<T>>;

/// A deque from the same pool. It keeps what it holds where it was put:
/// growing it moves nothing, so a pointer to an element lives as long as the
/// element does.
template <typename T>
using sta_deque = std::deque<T, sta_allocator<T>>;

}  // export namespace wxl::core
