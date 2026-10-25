#pragma once

// wxl::test::sta_box<T> -- a T in the STA pool, for a test that destroys an object at a moment of
// its own choosing: std::unique_ptr<sta_box<T>>, and reset() is the end of the object's life. An
// optional said the same thing in a way a reader takes for "a value that may be absent", which is
// not the question such a test asks.
//
// The T is a member rather than a base: what a test destroys this way is often final (a
// change_watch). The box's operator new and delete take the pool through its form for a size
// fixed at compile time, the one meant for a class's own new and delete: sizeof the box, so the
// call is a table load and the block goes back to the size class it came from. The box is final,
// so that size is always the object's. It lives on the pool's thread and inside the pool's life,
// as every test of an STA object does already.
//
// No standard header: the test that includes this one may have imported the std module already.

import wxl.core;

namespace wxl::test {

template <class T>
struct sta_box final {
    template <class... Args>
    explicit sta_box(Args&&... args) : value(static_cast<Args&&>(args)...) {}

    static void* operator new(decltype(sizeof 0)) { return core::sta_memory_pool::alloc<sizeof(sta_box)>(); }

    static void operator delete(void* memory) noexcept { core::sta_memory_pool::free<sizeof(sta_box)>(memory); }

    T value;
};

}  // namespace wxl::test
