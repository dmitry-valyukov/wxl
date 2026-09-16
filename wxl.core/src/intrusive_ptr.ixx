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
    using element_type = T;

    constexpr intrusive_ptr() noexcept = default;

    constexpr intrusive_ptr(std::nullptr_t) noexcept {}

    /// Implicit by design -- `return new foo();` into an `intrusive_ptr<foo>` has to work.
    /// \param add_ref false adopts a reference the caller already owns instead of taking
    ///                a new one (objects deriving from refcounted start at a count of 1).
    intrusive_ptr(T* p, bool add_ref = true) noexcept : ptr_(p) {
        if (ptr_ && add_ref) intrusive_ptr_add_ref(ptr_);
    }

    intrusive_ptr(const intrusive_ptr& other) noexcept : ptr_(other.ptr_) {
        if (ptr_) intrusive_ptr_add_ref(ptr_);
    }

    intrusive_ptr(intrusive_ptr&& other) noexcept : ptr_(std::exchange(other.ptr_, nullptr)) {}

    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr(const intrusive_ptr<U>& other) noexcept : ptr_(other.get()) {
        if (ptr_) intrusive_ptr_add_ref(ptr_);
    }

    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr(intrusive_ptr<U>&& other) noexcept : ptr_(other.detach()) {}

    ~intrusive_ptr() {
        if (ptr_) intrusive_ptr_release(ptr_);
    }

    intrusive_ptr& operator=(const intrusive_ptr& other) noexcept {
        intrusive_ptr(other).swap(*this);
        return *this;
    }

    intrusive_ptr& operator=(intrusive_ptr&& other) noexcept {
        intrusive_ptr(std::move(other)).swap(*this);
        return *this;
    }

    template <class U>
        requires(!std::same_as<U, T>) && std::convertible_to<U*, T*>
    intrusive_ptr& operator=(const intrusive_ptr<U>& other) noexcept {
        intrusive_ptr(other).swap(*this);
        return *this;
    }

    intrusive_ptr& operator=(T* p) noexcept {
        intrusive_ptr(p).swap(*this);
        return *this;
    }

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

    [[nodiscard]] T* get() const noexcept { return ptr_; }

    T& operator*() const noexcept {
        assume(ptr_);
        return *ptr_;
    }

    T* operator->() const noexcept {
        assume(ptr_);
        return ptr_;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

    [[nodiscard]] bool operator!() const noexcept { return ptr_ == nullptr; }

    void swap(intrusive_ptr& other) noexcept { std::swap(ptr_, other.ptr_); }

private:
    T* ptr_ = nullptr;
};

/**
 * Owns one reference to an intrusively counted object and does nothing else with it: it
 * takes the reference it is handed and releases it when it goes.
 *
 * Deliberately poorer than `intrusive_ptr` -- no copying, no moving, no reassignment, no
 * `intrusive_ptr_add_ref` anywhere in it. That is the whole point: a field set once in the
 * member initializer and released with its owner, where a second reference would be a
 * mistake rather than a convenience. `component` holds its state this way, so the state's
 * reference count reads as "one for the component, plus one per keeper out there".
 */
template <class T>
class release_only_ptr
{
public:
    /// Takes over `ptr`'s existing reference -- the implicit first one, if `ptr` was just
    /// created (refcounted objects start at ref_count() == 1).
    explicit release_only_ptr(T* ptr) noexcept : ptr_(ptr) {}

    ~release_only_ptr() {
        if (ptr_) intrusive_ptr_release(ptr_);
    }

    [[nodiscard]] T* get() const noexcept { return ptr_; }

    T* operator->() const noexcept {
        assume(ptr_);
        return ptr_;
    }

    T& operator*() const noexcept {
        assume(ptr_);
        return *ptr_;
    }

    [[nodiscard]] explicit operator bool() const noexcept { return ptr_ != nullptr; }

private:
    // const, so that the absence of assignment is the type's own doing rather than a rule
    // to remember; it makes the class non-copyable and non-movable at the same time.
    T* const ptr_;
};

// operator!=, <, <=, > and >= are all synthesized from these.
template <class T, class U>
bool operator==(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept {
    return a.get() == b.get();
}

template <class T>
bool operator==(const intrusive_ptr<T>& p, std::nullptr_t) noexcept {
    return !p;
}

template <class T, class U>
auto operator<=>(const intrusive_ptr<T>& a, const intrusive_ptr<U>& b) noexcept {
    return a.get() <=> b.get();
}

}  // export namespace wxl::core
