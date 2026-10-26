module;

#include "abi.h"

export module wxl.core:not_null;

import :checks;
import :intrusive_ptr;
import std;

export namespace wxl::core {

/**
 * A pointer that is not null, and says so in the type: a function taking one
 * has already been told, by whoever called it, that there is something there.
 *
 * A null pointer handed in is not bad input but a bug in the program that
 * handed it in, so it is not reported back to be handled -- there is nothing
 * sensible to do with the answer. It names the reason and aborts, in every
 * build, the way `ensure()` does everywhere else here.
 */
template <class T>
class not_null
{
public:
    /// Constructs a `not_null` that holds \p ptr. A null \p ptr aborts the process in every
    /// build, as `ensure()` does.
    explicit not_null(T* ptr) : ptr_(ptr) { ensure(ptr && "not_null was given a null pointer"); }

    /// Constructs a `not_null` from a `not_null` to a type whose pointer converts to `T*`.
    template <class T2>
        requires std::convertible_to<T2*, T*>
    not_null(const not_null<T2>& ptr) : ptr_(ptr.get()) {}

    /// Returns the held pointer, which is never null.
    [[nodiscard]] T* get() const noexcept {
        T* tmp = ptr_;
        assume(tmp);
        return tmp;
    }

    /// Returns the held pointer for member access.
    [[nodiscard]] T* operator->() const noexcept { return get(); }

    /// Returns a reference to the object the held pointer refers to.
    [[nodiscard]] T& operator*() const {
        T* tmp = ptr_;
        assume(tmp);
        return *tmp;
    }

    /// What is held, in the type that admits to being nullable -- for handing
    /// it on to an API written in that type. Every specialization has this
    /// conversion; for a plain pointer the nullable version is the pointer.
    explicit operator T*() const noexcept { return get(); }

private:
    T* const ptr_;
};

/* ------------------------------------------------------ */
/// not_null specialization for `const void`: an opaque, non-dereferenceable handle.
/// T& operator*() cannot exist for a void type, so it's omitted here.
template <>
class not_null<const void>
{
public:
    /// Constructs a `not_null` that holds \p ptr. A null \p ptr aborts the process in every
    /// build, as `ensure()` does.
    inline explicit not_null(const void* ptr) : ptr_(ptr) {
        ensure(ptr && "not_null was given a null pointer");
    }

    /// Constructs a `not_null` from the pointer held by another `not_null`.
    template <class T2>
    not_null(const not_null<T2>& ptr) : ptr_(ptr.get()) {}

    /// Returns the held pointer, which is never null.
    [[nodiscard]] inline const void* get() const noexcept {
        const void* tmp = ptr_;
        assume(tmp);
        return tmp;
    }

    /// Returns the held pointer as `const void*`, the nullable type, for passing it to an API
    /// written in that type.
    inline explicit operator const void*() const noexcept { return get(); }

private:
    const void* const ptr_;
};

/// Free get_pointer(), for generic code that dereferences smart pointers uniformly.
template <class T>
T* get_pointer(const not_null<T>& p) {
    return p.get();
}

/// Alternative for not_null<SomeVeryLongOrUndeductiveTypeName>(ptr): the
/// type is deduced from the pointer instead of being written out. Aborts, like
/// the constructor it stands for, when the pointer is null.
template <class T>
[[nodiscard]] not_null<T> as_not_null(T* ptr) {
    return not_null<T>(ptr);
}

/* ------------------------------------------------------ */
/// not_null specialization for intrusive_ptr
template <class T>
class not_null<intrusive_ptr<T>>
{
public:
    /// The type of the held smart pointer, `intrusive_ptr<T>`.
    using SmartPtr = intrusive_ptr<T>;

    /// Constructs a `not_null` that adopts the reference \p ptr already holds instead of adding
    /// a second one: objects derived from `wxl::core::refcounted` or `wxl::core::refcounted_mt`
    /// start with a reference count of 1. A null \p ptr aborts the process in every build.
    explicit not_null(T* ptr) : ptr_(ptr, false) { ensure(ptr && "not_null was given a null pointer"); }

    /// Constructs a `not_null` from another `not_null` and adds a reference to the object.
    ///
    /// The pointer is obtained through `operator->`, which returns the element pointer whatever
    /// the source holds, so one constructor serves a `not_null` over a raw pointer and one over a
    /// smart pointer.
    template <class T2>
    not_null(const not_null<T2>& ptr) : ptr_(ptr.operator->()) {}

    /// Constructs a `not_null` from \p ptr and adds a reference to the object. A null \p ptr
    /// aborts the process in every build.
    template <class T2>
    explicit not_null(const intrusive_ptr<T2>& ptr) : ptr_(ptr) {
        ensure(ptr && "not_null was given a null intrusive_ptr");
    }

    /// Returns the held object as a `not_null` raw pointer. The reference count does not change.
    [[nodiscard]] not_null<T> get() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return not_null<T>(tmp);
    }

    /// Returns the raw pointer to the held object for member access.
    [[nodiscard]] T* operator->() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return tmp;
    }

    /// Converts to `intrusive_ptr<T>`, the nullable type, and adds a reference to the object.
    operator SmartPtr() const { return ptr_; }

private:
    SmartPtr ptr_;
};

/* ------------------------------------------------------ */
/// Tests whether \p l and \p r refer to the same object. The compiler synthesizes
/// `operator!=` from this operator.
template <class T>
bool operator==(const not_null<T>& l, const not_null<T>& r) {
    return l.get() == r.get();
}

/// Compares the pointers held by \p l and \p r. The compiler synthesizes `operator<`,
/// `operator<=`, `operator>` and `operator>=` from this operator.
template <class T>
auto operator<=>(const not_null<T>& l, const not_null<T>& r) {
    return l.get() <=> r.get();
}

}  // export namespace wxl::core
