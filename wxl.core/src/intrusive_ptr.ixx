module;

#include "abi.h"

export module wxl.core:intrusive_ptr;

import std;

export namespace wxl::core {

/**
 * Intrusive smart pointer: the reference count lives inside the pointee rather than in a
 * separate control block, and is manipulated through the ADL-found hooks
 * `intrusive_ptr_add_ref(T*)` / `intrusive_ptr_release(T*)`. `refcounted` and
 * `refcounted_mt` declare both as hidden friends, so any type deriving from either is
 * usable here without further work.
 *
 * Both hooks take a `const T*`, which is what lets `intrusive_ptr<const T>` work -- the
 * shared states behind `future` are held that way.
 *
 * Shaped like the `boost::intrusive_ptr` it replaces: the same implicit construction from a
 * raw pointer, the same `intrusive_ptr(T*, bool add_ref)` adopt form, and the same
 * `detach()`.
 */
template <class T>
class intrusive_ptr
{
public:
    /// The type of the object the pointer refers to.
    using element_type = T;

    /// Constructs an empty pointer.
    constexpr intrusive_ptr() noexcept = default;

    /// Constructs an empty pointer.
    constexpr intrusive_ptr(std::nullptr_t) noexcept {}

    /// Implicit by design -- `return new foo();` into an `intrusive_ptr<foo>` has to work.
    /// \param add_ref false adopts a reference the caller already owns instead of taking
    ///                a new one (objects deriving from refcounted start at a count of 1).
    intrusive_ptr(T* p, bool add_ref = true) noexcept : ptr_(p) {
        if (ptr_ && add_ref) intrusive_ptr_add_ref(ptr_);
    }

    /// Constructs a pointer to the object held by \p other and adds a reference to it, if any.
    intrusive_ptr(const intrusive_ptr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) intrusive_ptr_add_ref(ptr_);
    }

    /// Constructs a pointer that takes over the reference held by \p other and leaves \p other
    /// empty.
    intrusive_ptr(intrusive_ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    /// Constructs a pointer from a pointer to a type whose pointer converts to `T*`, and adds a
    /// reference to the object, if any.
    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr(const intrusive_ptr<U>& other) noexcept : ptr_(other.get()) {
        if (ptr_) intrusive_ptr_add_ref(ptr_);
    }

    /// Constructs a pointer from a pointer to a type whose pointer converts to `T*`, taking over
    /// the reference held by \p other and leaving \p other empty.
    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr(intrusive_ptr<U>&& other) noexcept : ptr_(other.detach()) {}

    ~intrusive_ptr() {
        if (ptr_) intrusive_ptr_release(ptr_);
    }

    /// Replaces the held object with the one held by \p other: adds a reference to the new
    /// object, then releases the previous one.
    intrusive_ptr& operator=(const intrusive_ptr& other) noexcept {
        intrusive_ptr(other).swap(*this);
        return *this;
    }

    /// Takes over the reference held by \p other, leaves \p other empty and releases the
    /// previously held object.
    intrusive_ptr& operator=(intrusive_ptr&& other) noexcept {
        intrusive_ptr(std::move(other)).swap(*this);
        return *this;
    }

    /// Replaces the held object with the one held by \p other, a pointer to a type whose pointer
    /// converts to `T*`: adds a reference to the new object, then releases the previous one.
    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr& operator=(const intrusive_ptr<U>& other) noexcept {
        intrusive_ptr(other).swap(*this);
        return *this;
    }

    /// Replaces the held object with \p p: adds a reference to \p p, if not null, then releases
    /// the previous object.
    intrusive_ptr& operator=(T* p) noexcept {
        intrusive_ptr(p).swap(*this);
        return *this;
    }

    /// Releases the held object, if any, and leaves the pointer empty.
    intrusive_ptr& operator=(std::nullptr_t) noexcept {
        intrusive_ptr().swap(*this);
        return *this;
    }

    /// Takes ownership of `p`, releasing whatever was held before.
    /// \param add_ref see the constructor.
    void reset(T* p = nullptr, bool add_ref = true) noexcept {
        intrusive_ptr(p, add_ref).swap(*this);
    }

    /// Gives up ownership *without* releasing: the caller inherits this instance's reference
    /// and becomes responsible for it (pair with `intrusive_ptr(p, false)`).
    [[nodiscard]] T* detach() noexcept { return std::exchange(ptr_, nullptr); }

    /// Returns the stored pointer, which may be null. The reference count does not change.
    [[nodiscard]] T* get() const noexcept { return ptr_; }

    /// Returns a reference to the held object. The pointer must not be empty; this is asserted
    /// in debug builds and assumed in release builds.
    T& operator*() const noexcept {
        assume(ptr_);
        return *ptr_;
    }

    /// Returns the stored pointer for member access. The pointer must not be empty; this is
    /// asserted in debug builds and assumed in release builds.
    T* operator->() const noexcept {
        assume(ptr_);
        return ptr_;
    }

    /// Tests whether the pointer holds an object.
    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    /// Tests whether the pointer is empty.
    [[nodiscard]] bool operator!() const noexcept { return ptr_ == nullptr; }

    /// Exchanges the stored pointers of this object and \p other. No reference count changes.
    void swap(intrusive_ptr& other) noexcept { std::swap(ptr_, other.ptr_); }

private:
    T* ptr_ = nullptr;
};

/// Tests whether \p a and \p b store the same pointer. The compiler synthesizes `operator!=`
/// from this operator.
template <class T, class U>
bool operator==(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept {
    return a.get() == b.get();
}

/// Tests whether \p p is empty. The compiler synthesizes the reversed and the negated forms
/// from this operator.
template <class T>
bool operator==(const intrusive_ptr<T>& p, std::nullptr_t) noexcept {
    return !p;
}

/// Compares the pointers stored in \p a and \p b. The compiler synthesizes `operator<`,
/// `operator<=`, `operator>` and `operator>=` from this operator.
template <class T, class U>
auto operator<=>(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept {
    return a.get() <=> b.get();
}

}  // export namespace wxl::core
