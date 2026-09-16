// The single place wxl.xml reaches wxl.core, and the reason every other
// partition here imports this one instead of importing it directly.
//
// It is re-exported rather than kept inside: a tree's children are a
// wxl::core::intrusive_slist, and the pool everything here allocates from is
// built by whoever uses the reader, so a consumer of wxl.xml needs wxl.core
// anyway and should not have to name it twice.

export module wxl.xml:core;

import std;

export import wxl.core;

export namespace wxl::xml {

// The aliases below differ from their std counterparts in one way: the
// allocator. sta_allocator hands out memory from the STA pool, 10-15x faster
// than the default heap and legitimate here because everything this reader
// allocates for its own bookkeeping is created, used and destroyed on the
// single main STA thread.
//
// Worth having only where allocation actually repeats. A buffer the parse
// keeps and refills grows once and then never allocates again, so the pool
// would save it a single call per document: those members are ordinary std::
// containers, and so is anything a document decides the size of, which a pool
// for small objects has nothing to give anyway. As it stands the reader takes
// nothing per call either -- the tree it builds lives in the arena, and the
// walks over it allocate nothing -- so these are here for whoever needs to
// say "a container on the pool", not because the reader is holding one.
/// @{
template <typename T>
using sta_vector = std::vector<T, core::sta_allocator<T>>;

using sta_wstring = std::basic_string<wchar_t, std::char_traits<wchar_t>, core::sta_allocator<wchar_t>>;

using sta_string = std::basic_string<char, std::char_traits<char>, core::sta_allocator<char>>;
/// @}

}  // namespace wxl::xml
