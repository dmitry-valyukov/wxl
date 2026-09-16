module;

#include "abi.h"

export module wxl.core:pool_ptr;

import std;

export namespace wxl::core {

template <class t_element>
struct default_deleter {
    default_deleter() = default;

    // construct from another DefaultDeleter
    template <class t_other>
    explicit default_deleter(const default_deleter<t_other>&) {}

    void operator()(t_element* element) const noexcept {
        assert(element);
        delete element;
    }
};

/// Deleter for pool elements
template <class t_element, class t_base_deleter = default_deleter<t_element>>
struct intrusive_owner_deleter : t_base_deleter {
    using base = t_base_deleter;

    intrusive_owner_deleter() = default;

    // construct from another IntrusiveOwnerDeleter
    template <class t_other>
    explicit intrusive_owner_deleter(const intrusive_owner_deleter<t_other>&) {}

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

template <class t_owner_type>
class provided_owner_deleter
{
public:
    explicit provided_owner_deleter(t_owner_type* owner) noexcept : owner_(owner) {
        assert(owner_);
    }

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
    using deleter_type = t_deleter;
    using element_type = t_element;
    using my_type = pool_ptr<t_element>;

    /// Explicit \c std::unique_ptr like constructor
    explicit pool_ptr(element_type* ptr = nullptr, t_deleter deleter = t_deleter()) noexcept
        : t_deleter(std::move(deleter)), ptr_(ptr) {}

    ~pool_ptr() {
        if (element_type* const my_ptr = ptr_) free(my_ptr);
    }

    pool_ptr(const pool_ptr&) = delete;
    pool_ptr& operator=(const pool_ptr&) = delete;

    pool_ptr(pool_ptr&& x) noexcept : t_deleter(std::move(x)), ptr_(std::exchange(x.ptr_, nullptr)) {}

    pool_ptr& operator=(pool_ptr&& x) noexcept {
        this->t_deleter::operator=(std::move(x));
        reset(x.release());

        return *this;
    }

    template <typename T1, typename T2>
        requires std::convertible_to<T1*, t_element*> && std::convertible_to<T2, t_deleter>
    pool_ptr(pool_ptr<T1, T2>&& x) noexcept
        : t_deleter(std::move(x)), ptr_(std::exchange(x.ptr_, nullptr)) {}

    template <typename T1, typename T2>
        requires std::convertible_to<T1*, t_element*> && std::convertible_to<T2, t_deleter>
    pool_ptr& operator=(pool_ptr<T1, T2>&& x) noexcept {
        this->t_deleter::operator=(std::move(x));
        reset(x.release());

        return *this;
    }

    element_type* operator->() const { return checked_ptr(); }

    element_type* get() const noexcept { return ptr_; }

    element_type& operator*() { return *checked_ptr(); }

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
