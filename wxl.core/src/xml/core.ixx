// The containers of the reader's namespace that allocate from the STA pool.

export module wxl.core:xml.core;

import :sta_allocator;
import std;

export namespace wxl::xml {

/// @{
/// The type of a `std::vector` that allocates from the STA pool through
/// `wxl::core::sta_allocator`.
///
/// The aliases in this group differ from their standard counterparts only in the allocator.
/// The STA pool is 10 to 15 times faster than the default heap, and it is usable here because
/// everything that the reader allocates for its own use is created, used and
/// destroyed on the single STA thread.
///
/// The pool is used only where allocation repeats. A buffer that the parse keeps and refills
/// grows once and then does not allocate again, so the pool would save it one call per
/// document; such members are ordinary standard containers. So is any container whose size a
/// document decides, since a pool for small objects gives nothing at that size. The reader
/// itself allocates nothing per call: the tree lives in the arena, and the walks over it
/// allocate nothing. The aliases are provided for code that needs a container on the pool,
/// not because the reader holds one.
template <typename T>
using sta_vector = std::vector<T, core::sta_allocator<T>>;

/// The type of a `std::wstring` that allocates from the STA pool through
/// `wxl::core::sta_allocator`.
using sta_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, core::sta_allocator<wchar_t>>;

/// The type of a `std::string` that allocates from the STA pool through
/// `wxl::core::sta_allocator`.
using sta_string = std::basic_string<char, std::char_traits<char>, core::sta_allocator<char>>;
/// @}

}  // namespace wxl::xml
