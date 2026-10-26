export module wxl.core:sync_root;

import :lock_guard;
import :mutex;
import :not_null;
import :noncopyable;
import :refcounted;
import :intrusive_ptr;

export namespace wxl::core {

/// Represents a synchronization root for components: a `wxl::core::recursive_mutex` with an
/// intrusive reference count that can be updated from multiple threads.
class sync_root final : public recursive_mutex, public refcounted_mt
{
public:
    /// The type of the mutex that a sync root is.
    using mutex = recursive_mutex;

    /// The type of the scoped guard that acquires the sync root on construction and releases
    /// it on destruction. The guard does not add to the reference count of the sync root, so
    /// it does not keep the sync root alive, and it cannot release the lock before its
    /// destruction.
    using guard = lock_guard<recursive_mutex>;

    /// The type of the scoped guard that can release and reacquire the sync root during its
    /// lifetime.
    using unlockable_guard = unlockable_guard<recursive_mutex>;

    /// Gets the current reference count, read with relaxed memory ordering.
    using refcounted_mt::ref_count;

    sync_root() noexcept = default;

protected:
    ~sync_root() = default;
};

/// The type that holds a counted, non-null reference to a `wxl::core::sync_root`.
using sync_root_holder = not_null<intrusive_ptr<sync_root>>;

/// Provides a scoped guard that acquires a `wxl::core::sync_root` and holds a reference to
/// it. The reference keeps the sync root alive while the guard exists, even after the owner
/// of the sync root is destroyed. The lock is recursive.
class sync_root_guard : public noncopyable
{
public:
    /// Constructs the guard, takes a reference to \p root and acquires it.
    inline explicit sync_root_guard(not_null<sync_root> root) : sync_root_(root), g_(*root) {}

    /// Constructs the guard, takes a reference to the sync root that \p sync_root holds and
    /// acquires it.
    inline explicit sync_root_guard(const sync_root_holder& sync_root)
        : sync_root_(sync_root.get()), g_(*sync_root.get()) {}

private:
    sync_root_holder const sync_root_;
    sync_root::guard g_;
};

/// Provides a scoped guard over a `wxl::core::sync_root` that holds a reference to it and can
/// release and reacquire the lock during its lifetime. The reference keeps the sync root
/// alive while the guard exists, even after the owner of the sync root is destroyed. The
/// destructor releases the lock only if the guard holds it.
class unlockable_sync_root_guard : public noncopyable
{
public:
    /// Constructs the guard, takes a reference to \p root and acquires it if
    /// \p acquire_lock is `true`.
    inline explicit unlockable_sync_root_guard(not_null<sync_root> root, bool acquire_lock = true)
        : sync_root_(root), g_(*root, acquire_lock) {}

    /// Constructs the guard, takes a reference to the sync root that \p sync_root holds and
    /// acquires it if \p acquire_lock is `true`.
    inline explicit unlockable_sync_root_guard(const sync_root_holder& sync_root,
                                               bool acquire_lock = true)
        : sync_root_(sync_root.get()), g_(*sync_root.get(), acquire_lock) {}

    /// Acquires the sync root, blocking until it is available. The guard must not hold it.
    inline void lock() { g_.lock(); }

    /// Attempts to acquire the sync root without blocking. The guard must not hold it.
    /// \return `true` if the sync root was acquired.
    inline bool try_lock() { return g_.try_lock(); }

    /// Releases the sync root. The guard must hold it.
    inline void unlock() { g_.unlock(); }

private:
    sync_root_holder const sync_root_;
    sync_root::unlockable_guard g_;
};

/// Holds a reference to a `wxl::core::sync_root`, creating a new sync root when none is
/// supplied.
class auto_sync_root_holder : public sync_root_holder
{
    using base = sync_root_holder;

public:
    /// Constructs a holder that takes a reference to \p root, or creates a new sync root if
    /// \p root is empty.
    explicit auto_sync_root_holder(nullable<sync_root> root = nullptr);
    /// Constructs a holder that takes a reference to \p sync_root.
    explicit auto_sync_root_holder(not_null<sync_root> sync_root);
    /// Constructs a holder that shares the sync root of \p sync_root.
    explicit auto_sync_root_holder(const sync_root_holder& sync_root);
    ~auto_sync_root_holder();
};

/// Provides a base class for objects synchronized through a `wxl::core::sync_root`, which
/// several objects can share, and the guard types that lock it.
class synchronized_ : public noncopyable
{
protected:
    /// Constructs the object over \p root, or over a new sync root if \p root is empty.
    explicit synchronized_(nullable<core::sync_root> root = nullptr);
    /// Constructs the object over \p sync_root.
    explicit synchronized_(not_null<core::sync_root> sync_root);
    /// Constructs the object over the sync root that \p sync_root holds.
    explicit synchronized_(const sync_root_holder& sync_root);

public:
    /// The type of the mutex that the sync root is.
    using mutex = sync_root::mutex;

    virtual ~synchronized_();

    /// Gets the sync root of the object.
    inline not_null<core::sync_root> sync_root() const { return sync_root_.get(); }

    /// Provides a scoped guard that acquires a sync root on construction and releases it on
    /// destruction.
    class guard : public sync_root::guard
    {
        using base = sync_root::guard;

    public:
        /// Constructs the guard and acquires the sync root of \p self.
        inline explicit guard(not_null<const synchronized_> self) : base(*self->sync_root()) {}

        /// Constructs the guard and acquires the sync root of \p self.
        inline explicit guard(const synchronized_& self) : base(*self.sync_root()) {}

        /// Constructs the guard and acquires \p sync_root.
        inline explicit guard(not_null<core::sync_root> sync_root) : base(*sync_root) {}
    };

    /// Provides a scoped guard that acquires a sync root on request and releases it on
    /// destruction in every case.
    class lock_guard : public sync_root::guard
    {
        using base = sync_root::guard;

    public:
        /// Constructs the guard over the sync root of \p self and acquires it if
        /// \p acquire_lock is `true`. The destructor releases the sync root in either case,
        /// so with `false` the caller must already hold it.
        inline explicit lock_guard(not_null<const synchronized_> self, bool acquire_lock = true)
            : base(*self->sync_root(), acquire_lock) {}

        /// Constructs the guard over the sync root of \p self and acquires it if
        /// \p acquire_lock is `true`. The destructor releases the sync root in either case,
        /// so with `false` the caller must already hold it.
        inline explicit lock_guard(const synchronized_& self, bool acquire_lock = true)
            : base(*self.sync_root(), acquire_lock) {}

        /// Constructs the guard over \p sync_root and acquires it if \p acquire_lock is
        /// `true`. The destructor releases \p sync_root in either case, so with `false` the
        /// caller must already hold it.
        inline explicit lock_guard(not_null<core::sync_root> sync_root, bool acquire_lock = true)
            : base(*sync_root, acquire_lock) {}
    };

    /// Provides a scoped guard that can release and reacquire a sync root during its
    /// lifetime.
    class unlockable_guard : public sync_root::unlockable_guard
    {
        using base = sync_root::unlockable_guard;

    public:
        /// Constructs the guard over the sync root of \p self and acquires it if
        /// \p acquire_lock is `true`.
        inline explicit unlockable_guard(not_null<const synchronized_> self,
                                         bool acquire_lock = true)
            : base(*self->sync_root(), acquire_lock) {}

        /// Constructs the guard over the sync root of \p self and acquires it if
        /// \p acquire_lock is `true`.
        inline explicit unlockable_guard(const synchronized_& self, bool acquire_lock = true)
            : base(*self.sync_root(), acquire_lock) {}

        /// Constructs the guard over \p sync_root and acquires it if \p acquire_lock is
        /// `true`.
        inline explicit unlockable_guard(not_null<core::sync_root> sync_root,
                                         bool acquire_lock = true)
            : base(*sync_root, acquire_lock) {}
    };

    /// Provides a scoped guard that holds a reference to the sync root, so the sync root
    /// stays alive when the `wxl::core::synchronized_` object is destroyed.
    class safe_guard : public sync_root_guard
    {
    public:
        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it.
        inline explicit safe_guard(not_null<const synchronized_> self)
            : sync_root_guard(self->sync_root()) {}

        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it.
        inline explicit safe_guard(const synchronized_& self) : sync_root_guard(self.sync_root()) {}

    private:
        inline explicit safe_guard(not_null<core::sync_root> root) : sync_root_guard(root) {}
    };

    /// Provides a scoped guard that holds a reference to the sync root, so the sync root
    /// stays alive when the `wxl::core::synchronized_` object is destroyed. The guard can
    /// release and reacquire the sync root during its lifetime.
    class safe_lock_guard : public unlockable_sync_root_guard
    {
    public:
        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it if \p acquire_lock is `true`.
        inline explicit safe_lock_guard(not_null<const synchronized_> self,
                                        bool acquire_lock = true)
            : unlockable_sync_root_guard(self->sync_root(), acquire_lock) {}

        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it if \p acquire_lock is `true`.
        inline explicit safe_lock_guard(const synchronized_& self, bool acquire_lock = true)
            : unlockable_sync_root_guard(self.sync_root(), acquire_lock) {}

    private:
        inline explicit safe_lock_guard(not_null<core::sync_root> root, bool acquire_lock = true)
            : unlockable_sync_root_guard(root, acquire_lock) {}
    };

    /// Provides a scoped guard that holds a reference to the sync root, so the sync root
    /// stays alive when the `wxl::core::synchronized_` object is destroyed. The guard can
    /// release and reacquire the sync root during its lifetime.
    class safe_unlockable_guard : public unlockable_sync_root_guard
    {
    public:
        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it if \p acquire_lock is `true`.
        inline explicit safe_unlockable_guard(not_null<const synchronized_> self,
                                              bool acquire_lock = true)
            : unlockable_sync_root_guard(self->sync_root(), acquire_lock) {}

        /// Constructs the guard, takes a reference to the sync root of \p self and acquires
        /// it if \p acquire_lock is `true`.
        inline explicit safe_unlockable_guard(const synchronized_& self, bool acquire_lock = true)
            : unlockable_sync_root_guard(self.sync_root(), acquire_lock) {}

    private:
        inline explicit safe_unlockable_guard(not_null<core::sync_root> root,
                                              bool acquire_lock = true)
            : unlockable_sync_root_guard(root, acquire_lock) {}
    };

protected:
    /// Tests whether the calling thread owns the sync root.
    inline bool is_synchronized() const { return sync_root()->is_synchronized(); }

private:
    auto_sync_root_holder const sync_root_;
};

}  // export namespace wxl::core
