module;

#include "abi.h"

export module wxl.core:refcounted;

import :checks;
import :intrusive_ptr;
import :sta_allocator;
import std;

export namespace wxl::core {

// Single-threaded reference-counted base class
class refcounted
{
protected:
    refcounted() noexcept = default;
    refcounted(const refcounted&) = delete;
    refcounted& operator=(const refcounted&) = delete;

    virtual ~refcounted() noexcept = default;

    size_t ref_count() const noexcept {
        assume(ref_count_ > 0);
        return ref_count_;
    }

    void add_ref() const noexcept { ++ref_count_; }

    WXL_ALWAYS_INLINE
    void release_ref() const noexcept {
        if (--ref_count_ > 0) [[likely]]
            return;

        delete_this();
    }

private:
    WXL_NO_INLINE
    void delete_this() const noexcept { delete this; }

    friend void intrusive_ptr_add_ref(const refcounted* obj) noexcept { obj->add_ref(); }

    friend void intrusive_ptr_release(const refcounted* obj) noexcept { obj->release_ref(); }

    mutable size_t ref_count_{1};
};

// Multi-threaded reference-counted base class: same shape as `refcounted`,
// but the counter is atomic so add_ref()/release_ref() may be called
// concurrently from multiple threads. The decrement uses release ordering
// and only pays for an acquire fence on the path that actually deletes the
// object, instead of paying acquire on every release_ref() call.
class refcounted_mt
{
protected:
    refcounted_mt() noexcept = default;
    refcounted_mt(const refcounted_mt&) = delete;
    refcounted_mt& operator=(const refcounted_mt&) = delete;

    virtual ~refcounted_mt() noexcept = default;

    size_t ref_count() const noexcept {
        const size_t count = ref_count_.load(std::memory_order_relaxed);
        assume(count > 0);
        return count;
    }

    void add_ref() const noexcept { ref_count_.fetch_add(1, std::memory_order_relaxed); }

    WXL_ALWAYS_INLINE
    void release_ref() const noexcept {
        if (ref_count_.fetch_sub(1, std::memory_order_release) > 1) [[likely]]
            return;

        std::atomic_thread_fence(std::memory_order_acquire);
        delete_this();
    }

private:
    WXL_NO_INLINE
    void delete_this() const noexcept { delete this; }

    friend void intrusive_ptr_add_ref(const refcounted_mt* obj) noexcept { obj->add_ref(); }

    friend void intrusive_ptr_release(const refcounted_mt* obj) noexcept { obj->release_ref(); }

    mutable std::atomic<size_t> ref_count_{1};
};

// A counted object that lives in the STA pool -- which, on the one thread wxl
// works on, is the ordinary place for one.
//
// The pool is what wxl allocates from: a preset per description, a handler per
// subscription, an Impl per element, hundreds of them while an interface is
// built and gone again with it. Written as a base rather than as two lines
// repeated in every such class, because the two lines have to agree with each
// other and with the destructor, and that agreement is worth stating once.
//
// The pool's terms come with it, and they are the holder's to meet: an object
// is made and released on the pool's thread, and inside the pool's life. So
// nothing of this kind at namespace scope, where the constructor would run
// before the pool and the destructor after it -- one that must be written once
// and reused is written as a function returning it -- and a holder that
// outlives the pool gives its object back before the end, the way wWinMain
// does with its teardown handler and sta_loop::stop() with its wake-up.
class sta_refcounted : public refcounted
{
public:
    static void* operator new(std::size_t size) { return sta_memory_pool::alloc(size); }

    // Sized, and it has to be: the pool gives back to the size class it took
    // from. The size is the complete object's, because the destructor is
    // virtual and the deleting one the compiler writes knows which object it
    // is freeing.
    static void operator delete(void* mem, std::size_t size) noexcept {
        sta_memory_pool::free(mem, size);
    }
};

namespace impl {

// The count mixed into the type rather than wrapped around it: a pointer to
// this *is* a pointer to T, so `->` reaches T's own members and `*` binds to a
// `T&`, the way make_shared's pointer does. What it adds is the count and the
// allocator, and neither is anything the caller names.
template <class T>
class refcounted_mixin final : public sta_refcounted, public T
{
    static_assert(std::is_class_v<T> && !std::is_final_v<T>,
                  "wxl: make_refcounted mixes the count into the type itself, so the type "
                  "has to be a class that can be derived from");

    static_assert(!std::derived_from<T, refcounted> && !std::derived_from<T, refcounted_mt>,
                  "wxl: this type counts its own references already -- hold it as "
                  "intrusive_ptr<T> and make it with new");

public:
    // Everything is forwarded to T, including nothing at all.
    template <class... Args>
        requires std::constructible_from<T, Args...>
    explicit refcounted_mixin(Args&&... args) : T(std::forward<Args>(args)...) {}

    // Copying copies the value and nothing else: the count is what this class
    // adds, it belongs to the object rather than to the value, and a new
    // object starts its own at one. Written out rather than left to the
    // implicit pair, which refcounted deletes -- without these two, copying a
    // const one would be an error while copying a non-const one went through
    // the forwarding template above, and `*a = *b` would not compile at all.
    refcounted_mixin(const refcounted_mixin& other) : T(static_cast<const T&>(other)) {}

    refcounted_mixin& operator=(const refcounted_mixin& other) {
        static_cast<T&>(*this) = static_cast<const T&>(other);
        return *this;
    }

    // So that `*ptr = value` assigns the value: the pair above would otherwise
    // hide T's own assignment.
    using T::operator=;
};

}  // namespace impl

/**
 * A value shared by reference count, the way `std::make_shared` shares one: the pointer
 * that comes back points at a `T` -- `->` reaches its members, `*` is a `T&` -- and a copy
 * of the pointer is another reference to the same value, which goes when the last of them
 * does. Arguments are forwarded to `T`'s constructor; with none, it is default-constructed.
 *
 * What wants this is a value two parties share and neither owns. The case it was written
 * for is a subscription that takes itself off: the handler needs the token, and it is
 * called const, so it cannot keep one it was handed afterwards -- and the token does not
 * exist yet when the handler is written.
 *
 * ```cpp
 * auto const token = core::make_refcounted<EventToken>();
 * *token = element.add_onLayoutUpdated([element, token] {
 *     element.remove_onLayoutUpdated(*token);
 * });
 * ```
 *
 * The count is mixed into the type, so the type has to be a class that can be derived
 * from: not `int`, not a `final` one. A type that counts its own references belongs in
 * `intrusive_ptr<T>` instead, and says so.
 */
template <class T, class... Args>
    requires std::constructible_from<T, Args...>
intrusive_ptr<impl::refcounted_mixin<T>> make_refcounted(Args&&... args) {
    return intrusive_ptr<impl::refcounted_mixin<T>>{
        new impl::refcounted_mixin<T>(std::forward<Args>(args)...), /*add_ref=*/false};
}

}  // export namespace wxl::core
