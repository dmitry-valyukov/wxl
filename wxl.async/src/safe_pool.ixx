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

    /// The type of the underlying allocator: `base_allocator_t` instantiated for `element_type`.
    using base_allocator = base_allocator_t<element_type>;

    /// Makes object from base that can be used in SafePool.
    class element_type : public base_object_t, public core::intrusive_slist_node<element_type>
    {
        using base = base_object_t;

    public:
        /// Constructs an element that holds a reference to the shared state \p owner, with a
        /// default-constructed base object.
        explicit element_type(shared_pool_state* owner) noexcept : owner_(owner) {}

        /// Constructs an element that holds a reference to the shared state \p owner_arg, with
        /// the base object constructed from \p arg1.
        template <typename T1>
        element_type(shared_pool_state* owner_arg, const T1& arg1)
            : base(arg1), owner_(owner_arg) {}

        /// Gets the shared state of the pool the element belongs to.
        shared_pool_state* owner() noexcept { return owner_.get(); }

    private:
        core::intrusive_ptr<shared_pool_state> owner_;
    };

    /// Derived allocator for SafePool
    class pool_allocator : private base_allocator
    {
    public:
        /// The type of the allocated element.
        using element_type = typename base_allocator::element_type;

        /// Constructs the allocator for the shared state \p shared_state from a copy of the
        /// underlying allocator \p alloc.
        explicit pool_allocator(shared_pool_state* shared_state,
                                const base_allocator& alloc = base_allocator())
            : base_allocator(alloc), shared_state_(shared_state) {}

        /// Creates an element that belongs to the shared state.
        element_type* alloc() { return base_allocator::alloc(shared_state_); }

        /// Destroys \p element with the underlying allocator.
        void free(element_type* element) const { base_allocator::free(element); }

    private:
        shared_pool_state* shared_state_;
    };

    /// The type of the allocator the shared state uses.
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
        /// Constructs the shared state with the preferred number of elements \p preferred_size
        /// and the underlying allocator \p alloc.
        explicit shared_pool_state(size_t preferred_size = 256,
                                   const base_allocator& alloc = base_allocator()) noexcept
            : base(preferred_size, base::allocator(avoid_this_warning(), alloc)) {}

        /// Closes the pool and destroys the elements it holds (`wxl::async::pool::begin_dispose`).
        using base::begin_dispose;
    };

    /// Constructor.
    explicit safe_pool(size_t preferred_size = 256,
                       const base_allocator& alloc = base_allocator()) noexcept
        : state_(core::make_refcounted<shared_pool_state>(preferred_size, alloc)) {}

    ~safe_pool() {
        if (state_) state_->begin_dispose();
    }

    /// Gets the preferred number of elements in the pool.
    size_t preferred_size() const { return state_->preferred_size(); }

    /// Returns an element from the pool. When the pool is empty, creates one if
    /// \p create_if_needed is `true`, and returns `nullptr` otherwise.
    element_type* get(bool create_if_needed = true) { return state_->get(create_if_needed); }

    /// Puts \p element back into the pool. The element must belong to this pool.
    void put(element_type* element) {
        assert(element);
        assert(element->owner() == state_.get());

        state_->put(element);
    }

    /// Closes the pool, destroys the elements it holds and releases the reference to the shared
    /// state. Afterwards the pool holds no shared state, and only its destruction is valid.
    void begin_dispose() {
        state_->begin_dispose();
        state_.reset();
    }

private:
    core::intrusive_ptr<shared_pool_state> state_;
};

}  // export namespace wxl::async
