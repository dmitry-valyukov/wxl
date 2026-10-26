module;

#include "abi.h"

export module wxl.core:pool_ptr;

import std;

export namespace wxl::core {

/// Represents a deleter that destroys an element with `delete`. It is the default for the
/// `t_base_deleter` parameter of `wxl::core::intrusive_owner_deleter`, which passes it an element
/// that has no owner.
template <class t_element>
struct default_deleter {
    default_deleter() = default;

    /// Constructs a deleter from a `default_deleter` for another element type.
    template <class t_other>
    explicit default_deleter(const default_deleter<t_other>&) {}

    /// Destroys \p element with `delete`. \p element must not be null; this is asserted in
    /// debug builds.
    void operator()(t_element* element) const noexcept {
        assert(element);
        delete element;
    }
};

/// Deleter for pool elements
template <class t_element, class t_base_deleter = default_deleter<t_element>>
struct intrusive_owner_deleter : t_base_deleter {
    /// The type of the deleter that frees an element without an owner.
    using base = t_base_deleter;

    intrusive_owner_deleter() = default;

    /// Constructs a deleter from an `intrusive_owner_deleter` for another element type.
    template <class t_other>
    explicit intrusive_owner_deleter(const intrusive_owner_deleter<t_other>&) {}

    /// Returns \p element to the owner that `element->owner()` reports, through the owner's
    /// `put()`; an element whose owner is null is passed to the base deleter. \p element must
    /// not be null; this is asserted in debug builds.
    void operator()(t_element* element) const {
        assert(element);
        free_by_owner(element, element->owner());
    }

private:
    template <class t_owner_type>
    void free_by_owner(t_element* element, t_owner_type* owner) const {
        if (owner)
            owner->put(element);
        else
            base::operator()(element);
    }
};

/// Represents a deleter that returns every element to an owner supplied at construction,
/// through the owner's `put()`.
template <class t_owner_type>
class provided_owner_deleter
{
public:
    /// Constructs a deleter that returns elements to \p owner. \p owner must not be null; this
    /// is asserted in debug builds.
    explicit provided_owner_deleter(t_owner_type* owner) noexcept : owner_(owner) {
        assert(owner_);
    }

    /// Returns \p element to the owner through its `put()`. \p element must not be null; this
    /// is asserted in debug builds.
    void operator()(typename t_owner_type::element_type* element) const {
        assert(element);
        owner_->put(element);
    }

private:
    t_owner_type* const owner_;
};

/**
 * Smart pointer with semantics that is very close to std::unique_ptr but we do not
 * delete the object, instead we return it to the pool-owner.
 */
template <typename t_element, typename t_deleter = intrusive_owner_deleter<t_element>>
class pool_ptr : t_deleter
{
public:
    /// The type of the deleter, `t_deleter`.
    using deleter_type = t_deleter;
    /// The type of the element, `t_element`.
    using element_type = t_element;
    /// The type `pool_ptr<t_element>`, with the default deleter,
    /// `intrusive_owner_deleter<t_element>`.
    using my_type = pool_ptr<t_element>;

    /// Explicit \c std::unique_ptr like constructor
    explicit pool_ptr(element_type* ptr = nullptr, t_deleter deleter = t_deleter()) noexcept
        : t_deleter(std::move(deleter)), ptr_(ptr) {}

    ~pool_ptr() {
        if (element_type* const my_ptr = ptr_) free(my_ptr);
    }

    pool_ptr(const pool_ptr&) = delete;
    pool_ptr& operator=(const pool_ptr&) = delete;

    /// Constructs a `pool_ptr` that takes ownership of the element held by \p x and moves its
    /// deleter, leaving \p x empty.
    pool_ptr(pool_ptr&& x) noexcept : t_deleter(std::move(x)), ptr_(std::exchange(x.ptr_, nullptr)) {}

    /// Move-assigns the deleter from \p x, then takes ownership of the element held by \p x,
    /// leaving \p x empty. The previously held element, if any, is freed through the deleter
    /// taken from \p x.
    pool_ptr& operator=(pool_ptr&& x) noexcept {
        this->t_deleter::operator=(std::move(x));
        reset(x.release());

        return *this;
    }

    /// Constructs a `pool_ptr` from a `pool_ptr` whose element pointer and deleter convert to
    /// this type's, taking ownership of its element and moving its deleter. \p x is left empty.
    template <typename T1, typename T2>
        requires std::convertible_to<T1*, t_element*> && std::convertible_to<T2, t_deleter>
    pool_ptr(pool_ptr<T1, T2>&& x) noexcept
        : t_deleter(std::move(x)), ptr_(std::exchange(x.ptr_, nullptr)) {}

    /// Move-assigns from a `pool_ptr` whose element pointer and deleter convert to this type's:
    /// assigns the deleter, then takes ownership of the element held by \p x, leaving \p x
    /// empty. The previously held element, if any, is freed through the deleter taken from \p x.
    template <typename T1, typename T2>
        requires std::convertible_to<T1*, t_element*> && std::convertible_to<T2, t_deleter>
    pool_ptr& operator=(pool_ptr<T1, T2>&& x) noexcept {
        this->t_deleter::operator=(std::move(x));
        reset(x.release());

        return *this;
    }

    /// Returns the stored pointer for member access.
    /// \throw std::logic_error The pointer is empty, in builds with `WXL_CFG_DEBUG` defined.
    element_type* operator->() const { return checked_ptr(); }

    /// Returns the stored pointer, which may be null. No check is made.
    element_type* get() const noexcept { return ptr_; }

    /// Returns a reference to the held element.
    /// \throw std::logic_error The pointer is empty, in builds with `WXL_CFG_DEBUG` defined.
    element_type& operator*() { return *checked_ptr(); }

    /// Returns a const reference to the held element.
    /// \throw std::logic_error The pointer is empty, in builds with `WXL_CFG_DEBUG` defined.
    const element_type& operator*() const { return *checked_ptr(); }

    /// Returns the wrapped pointer and gives up ownership
    element_type* release() noexcept {
        element_type* my_ptr = ptr_;
        ptr_ = nullptr;
        return my_ptr;
    }

    /// Destroys the stored object and updates the internal pointer to the given one.
    void reset(element_type* ptr = nullptr) {
        element_type* const my_ptr = ptr_;

        if (my_ptr != ptr) {
            ptr_ = ptr;

            if (my_ptr) free(my_ptr);
        }
    }

private:
    template <typename, typename>
    friend class pool_ptr;

    t_deleter& deleter() noexcept { return *this; }

    void free(element_type* ptr) { deleter()(ptr); }

    element_type* checked_ptr() const {
        element_type* const my_ptr = ptr_;

#ifdef WXL_CFG_DEBUG
        if (!my_ptr) throw std::logic_error("Smart pointer pool_ptr<> is empty.");
#endif
        return my_ptr;
    }

private:
    element_type* ptr_;
};

}  // export namespace wxl::core
