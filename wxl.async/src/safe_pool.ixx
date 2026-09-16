module;

#include "abi.h"

export module wxl.async:safe_pool;

import :pool;
import wxl.core;

export namespace wxl::async {

/// Safe interthread pool.
/// This object shares the pool state between allocated objects and SafePool itself.
template <typename base_object_t, template <typename> class base_allocator_t = default_allocator>
class safe_pool : public core::noncopyable
{
public:
    class element_type;
    class shared_pool_state;

    using base_allocator = base_allocator_t<element_type>;

    /// Makes object from base that can be used in SafePool.
    class element_type : public base_object_t, public core::intrusive_slist_node<element_type>
    {
        using base = base_object_t;

    public:
        explicit element_type(shared_pool_state* owner) noexcept : owner_(owner) {}

        template <typename T1>
        element_type(shared_pool_state* owner_arg, const T1& arg1)
            : base(arg1), owner_(owner_arg) {}

        shared_pool_state* owner() noexcept { return owner_.get(); }

    private:
        core::intrusive_ptr<shared_pool_state> owner_;
    };

    /// Derived allocator for SafePool
    class pool_allocator : private base_allocator
    {
    public:
        using element_type = typename base_allocator::element_type;

        explicit pool_allocator(shared_pool_state* shared_state,
                                const base_allocator& alloc = base_allocator())
            : base_allocator(alloc), shared_state_(shared_state) {}

        element_type* alloc() { return base_allocator::alloc(shared_state_); }

        void free(element_type* element) const { base_allocator::free(element); }

    private:
        shared_pool_state* shared_state_;
    };

    using pool_allocator_type = pool_allocator;

    /// Pool's shared state.
    class shared_pool_state
        : public pool<element_type, pool_allocator_type, pool_access_policy_elements_dispose_mt>,
          public core::refcounted_mt
    {
        using base =
            pool<element_type, pool_allocator_type, pool_access_policy_elements_dispose_mt>;

        shared_pool_state* avoid_this_warning() { return this; }

    public:
        explicit shared_pool_state(size_t preferred_size = 256,
                                   const base_allocator& alloc = base_allocator()) noexcept
            : base(preferred_size, base::allocator(avoid_this_warning(), alloc)) {}

        using base::begin_dispose;
    };

    /// Constructor.
    explicit safe_pool(size_t preferred_size = 256,
                       const base_allocator& alloc = base_allocator()) noexcept
        : state_(new shared_pool_state(preferred_size, alloc)) {}

    ~safe_pool() {
        if (state_) state_->begin_dispose();
    }

    size_t preferred_size() const { return state_->preferred_size(); }

    element_type* get(bool create_if_needed = true) { return state_->get(create_if_needed); }

    void put(element_type* element) {
        assert(element);
        assert(element->owner() == state_.get());

        state_->put(element);
    }

    void begin_dispose() {
        state_->begin_dispose();
        state_.reset();
    }

private:
    core::intrusive_ptr<shared_pool_state> state_;
};

}  // export namespace wxl::async
