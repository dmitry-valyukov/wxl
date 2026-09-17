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
    explicit not_null(T* ptr) : ptr_(ptr) { ensure(ptr && "not_null was given a null pointer"); }

    template <class T2>
        requires std::convertible_to<T2*, T*>
    not_null(const not_null<T2>& ptr) : ptr_(ptr.get()) {}

    [[nodiscard]] T* get() const noexcept {
        T* tmp = ptr_;
        assume(tmp);
        return tmp;
    }

    [[nodiscard]] T* operator->() const noexcept { return get(); }

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
/// not_null specialization for `const void`: an opaque, non-dereferenceable handle
/// (e.g. wxl::core::cookie_t). T& operator*() cannot exist for a void type, so it's omitted here.
template <>
class not_null<const void>
{
public:
    inline explicit not_null(const void* ptr) : ptr_(ptr) {
        ensure(ptr && "not_null was given a null pointer");
    }

    template <class T2>
    not_null(const not_null<T2>& ptr) : ptr_(ptr.get()) {}

    [[nodiscard]] inline const void* get() const noexcept {
        const void* tmp = ptr_;
        assume(tmp);
        return tmp;
    }

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
    using SmartPtr = intrusive_ptr<T>;

    // Adopts ptr's already-implicit first reference (refcounted/refcounted_mt objects
    // start at ref_count() == 1 on construction) rather than adding a second one.
    explicit not_null(T* ptr) : ptr_(ptr, false) { ensure(ptr && "not_null was given a null pointer"); }

    template <class T2>
    // Through the arrow, which every wrapper answers with the element pointer
    // whatever it holds -- so one line serves a plain source and a smart one.
    not_null(const not_null<T2>& ptr) : ptr_(ptr.operator->()) {}

    template <class T2>
    explicit not_null(const intrusive_ptr<T2>& ptr) : ptr_(ptr) {
        ensure(ptr && "not_null was given a null intrusive_ptr");
    }

    [[nodiscard]] not_null<T> get() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return not_null<T>(tmp);
    }

    [[nodiscard]] T* operator->() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return tmp;
    }

    operator SmartPtr() const { return ptr_; }

private:
    SmartPtr ptr_;
};

/* ------------------------------------------------------ */
/// not_null specialization for release_only_ptr: the owning field that never was and
/// never will be null. Poorer than the others on purpose -- there is no conversion to the
/// smart pointer and no construction from one, because a release_only_ptr cannot be copied.
template <class T>
class not_null<release_only_ptr<T>>
{
public:
    using SmartPtr = release_only_ptr<T>;

    // Adopts ptr's already-implicit first reference, as the intrusive_ptr specialization
    // above does -- release_only_ptr never adds one.
    explicit not_null(T* ptr) : ptr_(ptr) { ensure(ptr && "not_null was given a null pointer"); }

    [[nodiscard]] not_null<T> get() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return not_null<T>(tmp);
    }

    [[nodiscard]] T* operator->() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return tmp;
    }

    [[nodiscard]] T& operator*() const noexcept { return *operator->(); }

    /// By reference, and only here: a release_only_ptr cannot be copied, so
    /// this is the one specialization whose conversion cannot hand out a copy.
    operator const SmartPtr&() const noexcept { return ptr_; }

private:
    SmartPtr ptr_;
};

/* ------------------------------------------------------ */
/// not_null specialization for shared_ptr
template <class T>
class not_null<std::shared_ptr<T>>
{
public:
    using SmartPtr = std::shared_ptr<T>;

    explicit not_null(T* ptr) : ptr_(ptr) { ensure(ptr && "not_null was given a null pointer"); }

    template <class T2>
    // Through the arrow, which every wrapper answers with the element pointer
    // whatever it holds -- so one line serves a plain source and a smart one.
    not_null(const not_null<T2>& ptr) : ptr_(ptr.operator->()) {}

    template <class T2>
    explicit not_null(const std::shared_ptr<T2>& ptr) : ptr_(ptr) {
        ensure(ptr && "not_null was given a null shared_ptr");
    }

    [[nodiscard]] not_null<T> get() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return not_null<T>(tmp);
    }

    [[nodiscard]] T* operator->() const noexcept {
        T* tmp = ptr_.get();
        assume(tmp);
        return tmp;
    }

    operator SmartPtr() const { return ptr_; }

private:
    SmartPtr ptr_;
};

/* ------------------------------------------------------ */
// operator!=, <, <=, > and >= are all synthesized from these two.
template <class T>
bool operator==(const not_null<T>& l, const not_null<T>& r) {
    return l.get() == r.get();
}

template <class T>
auto operator<=>(const not_null<T>& l, const not_null<T>& r) {
    return l.get() <=> r.get();
}

}  // export namespace wxl::core
