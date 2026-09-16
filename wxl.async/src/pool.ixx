module;

#include "abi.h"

export module wxl.async:pool;

import :mpsc_queue;
import :turnstile;
import wxl.core;
import std;

export namespace wxl::async {

template <class t_element>
struct default_allocator {
    using element_type = t_element;

    default_allocator() = default;

    template <class t_other>
    default_allocator(const default_allocator<t_other>&) {}

    template <typename... t_args>
        requires std::constructible_from<t_element, t_args...>
    static element_type* alloc(t_args&&... args) {
        return new element_type(std::forward<t_args>(args)...);
    }

    static void free(element_type* element) noexcept { delete element; }
};

/// Allows use of Pool::beginDispose
class pool_access_policy_elements_dispose_mt
{
public:
    bool try_enter() noexcept { return pool_access_.try_enter(); }

    void exit() noexcept { return pool_access_.exit(); }

    bool try_close() noexcept { return pool_access_.try_close(); }

    bool closed() const noexcept { return pool_access_.closed(); }

    template <class policy>
    static void ensure_begin_dispose_supported() noexcept {}

private:
    turnstile pool_access_;
};

/// Disallows use of `Pool::beginDispose`.
/// This reduces the amount of used atomic operations and thus faster `Pool::put`.
/// If `Pool::beginDispose` is used with this policy, the compilation fails.
class pool_access_policy_elements_dispose_st
{
public:
    static constexpr bool try_enter() noexcept { return true; }

    static void exit() noexcept {}

    static bool try_close() noexcept {
        assert(false);
        return true;
    }

    template <class policy>
    static void ensure_begin_dispose_supported() noexcept {
        static_assert(!std::is_same_v<policy, pool_access_policy_elements_dispose_st>,
                      "pool::begin_dispose is not supported, please use "
                      "pool_access_policy_elements_dispose_mt");
    }

    constexpr bool closed() const noexcept { return false; }
};

/**
 * Non blocking pool of objects by pointers.
 * Pool is safe for many writers and one reader.
 *
 * The good scenario is: producer of data creates the objects using the Pool
 * and sends them to consumer threads to process, then these threads returns
 * objects back to producer's Pool and so on.
 */
template <typename t_element, typename t_allocator = default_allocator<t_element>,
          typename pool_access_policy_elements_dispose = pool_access_policy_elements_dispose_mt>
class pool : public core::noncopyable
{
public:
    using element_type = t_element;
    using allocator = t_allocator;

    /// Initialize the Pool this desired average number of items in the pool.
    explicit pool(size_t preferred_size = 256, const allocator& allocator_arg = allocator())
        : preferred_size_(preferred_size),
          underflow_counter_(-static_cast<ssize_t>(preferred_size)),
          allocator_(allocator_arg) {}

    virtual ~pool() { free_elements(); }

    size_t preferred_size() const noexcept { return preferred_size_; }

    long long underflow_counter() const noexcept { return underflow_counter_; }

    /// @return An item from pool or create a new one if Pool is empty.
    element_type* get(bool create_if_needed = true);

    /// Returns the element to the pool.
    void put(element_type* item) {
        assert(item);

        if (!pool_access_.try_enter()) [[unlikely]] {
            allocator_.free(item);  // the pool is closed, so we need to dispose the item here.
            return;
        }

        // put item in the pool
        buffer_.push_one(item);

        pool_access_.exit();
    }

protected:
    allocator& get_allocator() noexcept { return allocator_; }

    element_type* alloc() { return get_allocator().alloc(); }

    void free_elements() {
        while (element_type* const element = buffer_.pop_one()) allocator_.free(element);
    }

    void begin_dispose() {
        pool_access_policy_elements_dispose::template ensure_begin_dispose_supported<
            pool_access_policy_elements_dispose>();

        assert(!pool_access_.closed());

        while (!pool_access_.try_close()) free_elements();

        free_elements();
    }

private:
    mpsc_stack<element_type> buffer_;
    pool_access_policy_elements_dispose pool_access_;

    // The alignment is added to prevent false sharing.
    // It happens when th1 get `poolAccess_` modified from `put`,
    // th2 reads `preferredSize_` from `get`
    alignas(std::hardware_destructive_interference_size) size_t const preferred_size_;
    ssize_t underflow_counter_;
    allocator allocator_;
};

// Pool<> members

template <typename t_element, typename t_allocator, typename pool_access_policy_elements_dispose>
t_element* pool<t_element, t_allocator, pool_access_policy_elements_dispose>::get(
    bool create_if_needed /*= true*/) {
    std::unique_ptr<element_type> item_ptr(buffer_.pop_one());

    if (item_ptr.get()) {
        if (buffer_.pops_since_claim() > preferred_size_) {
            // overflow detected, check underflowCounter_
            if (underflow_counter_ > 0)
                --underflow_counter_;
            else if (element_type* const element_to_delete = buffer_.pop_one())
                allocator_.free(element_to_delete);
        }

        return item_ptr.release();
    }

    if (create_if_needed) {
        ++underflow_counter_;
        return alloc();
    }

    return nullptr;
}

}  // export namespace wxl::async
