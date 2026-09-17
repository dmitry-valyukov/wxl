export module wxl.core:sync_root;

import :lock_guard;
import :mutex;
import :not_null;
import :noncopyable;
import :refcounted;
import :intrusive_ptr;

export namespace wxl::core {

/// Synchronization root for components.
/// It is an intrusive ref-counted object.
class sync_root final : public recursive_mutex, public refcounted_mt
{
public:
    using mutex = recursive_mutex;

    /// Simple and most effective guard
    using guard = lock_guard<recursive_mutex>;

    /// This guard can be unlocked and locked again
    using unlockable_guard = unlockable_guard<recursive_mutex>;

    using refcounted_mt::ref_count;

    sync_root() noexcept = default;

protected:
    ~sync_root() = default;
};

/// Shared holder of sync_root
using sync_root_holder = not_null<intrusive_ptr<sync_root>>;

/// This guard may hold reference to sync_root after its owner will be deleted.
/// For this purpose this is a normal recursive RAII guard.
class sync_root_guard : public noncopyable
{
public:
    inline explicit sync_root_guard(not_null<sync_root> root) : sync_root_(root), g_(*root) {}

    inline explicit sync_root_guard(const sync_root_holder& sync_root)
        : sync_root_(sync_root.get()), g_(*sync_root.get()) {}

private:
    sync_root_holder const sync_root_;
    sync_root::guard g_;
};

/// This guard may hold reference to sync_root after its owner will be deleted.
/// For this purpose this is a normal recursive RAII guard.
class unlockable_sync_root_guard : public noncopyable
{
public:
    inline explicit unlockable_sync_root_guard(not_null<sync_root> root, bool acquire_lock = true)
        : sync_root_(root), g_(*root, acquire_lock) {}

    inline explicit unlockable_sync_root_guard(const sync_root_holder& sync_root,
                                               bool acquire_lock = true)
        : sync_root_(sync_root.get()), g_(*sync_root.get(), acquire_lock) {}

    inline void lock() { g_.lock(); }

    inline bool try_lock() { return g_.try_lock(); }

    inline void unlock() { g_.unlock(); }

private:
    sync_root_holder const sync_root_;
    sync_root::unlockable_guard g_;
};

/// Creates its own sync_root exemplar if needed.
class auto_sync_root_holder : public sync_root_holder
{
    using base = sync_root_holder;

public:
    explicit auto_sync_root_holder(nullable<sync_root> root = nullptr);
    explicit auto_sync_root_holder(not_null<sync_root> sync_root);
    explicit auto_sync_root_holder(const sync_root_holder& sync_root);
    ~auto_sync_root_holder();
};

/// Base synchronized class.
class synchronized_ : public noncopyable
{
protected:
    explicit synchronized_(nullable<core::sync_root> root = nullptr);
    explicit synchronized_(not_null<core::sync_root> sync_root);
    explicit synchronized_(const sync_root_holder& sync_root);

public:
    using mutex = sync_root::mutex;

    virtual ~synchronized_();

    inline not_null<core::sync_root> sync_root() const { return sync_root_.get(); }

    /// Usual guard.
    class guard : public sync_root::guard
    {
        using base = sync_root::guard;

    public:
        inline explicit guard(not_null<const synchronized_> self) : base(*self->sync_root()) {}

        inline explicit guard(const synchronized_& self) : base(*self.sync_root()) {}

        inline explicit guard(not_null<core::sync_root> sync_root) : base(*sync_root) {}
    };

    /// Simple guard
    class lock_guard : public sync_root::guard
    {
        using base = sync_root::guard;

    public:
        inline explicit lock_guard(not_null<const synchronized_> self, bool acquire_lock = true)
            : base(*self->sync_root(), acquire_lock) {}

        inline explicit lock_guard(const synchronized_& self, bool acquire_lock = true)
            : base(*self.sync_root(), acquire_lock) {}

        inline explicit lock_guard(not_null<core::sync_root> sync_root, bool acquire_lock = true)
            : base(*sync_root, acquire_lock) {}
    };

    /// This guard can be unlocked and locked again
    class unlockable_guard : public sync_root::unlockable_guard
    {
        using base = sync_root::unlockable_guard;

    public:
        inline explicit unlockable_guard(not_null<const synchronized_> self,
                                         bool acquire_lock = true)
            : base(*self->sync_root(), acquire_lock) {}

        inline explicit unlockable_guard(const synchronized_& self, bool acquire_lock = true)
            : base(*self.sync_root(), acquire_lock) {}

        inline explicit unlockable_guard(not_null<core::sync_root> sync_root,
                                         bool acquire_lock = true)
            : base(*sync_root, acquire_lock) {}
    };

    /// This guard can hold the ownership of sync_root when synchronized_ is destroyed.
    class safe_guard : public sync_root_guard
    {
    public:
        inline explicit safe_guard(not_null<const synchronized_> self)
            : sync_root_guard(self->sync_root()) {}

        inline explicit safe_guard(const synchronized_& self) : sync_root_guard(self.sync_root()) {}

    private:
        inline explicit safe_guard(not_null<core::sync_root> root) : sync_root_guard(root) {}
    };

    /// This guard can hold the ownership of sync_root when synchronized_ is destroyed.
    class safe_lock_guard : public unlockable_sync_root_guard
    {
    public:
        inline explicit safe_lock_guard(not_null<const synchronized_> self,
                                        bool acquire_lock = true)
            : unlockable_sync_root_guard(self->sync_root(), acquire_lock) {}

        inline explicit safe_lock_guard(const synchronized_& self, bool acquire_lock = true)
            : unlockable_sync_root_guard(self.sync_root(), acquire_lock) {}

    private:
        inline explicit safe_lock_guard(not_null<core::sync_root> root, bool acquire_lock = true)
            : unlockable_sync_root_guard(root, acquire_lock) {}
    };

    /// This guard can hold the ownership of sync_root when synchronized_ is destroyed.
    class safe_unlockable_guard : public unlockable_sync_root_guard
    {
    public:
        inline explicit safe_unlockable_guard(not_null<const synchronized_> self,
                                              bool acquire_lock = true)
            : unlockable_sync_root_guard(self->sync_root(), acquire_lock) {}

        inline explicit safe_unlockable_guard(const synchronized_& self, bool acquire_lock = true)
            : unlockable_sync_root_guard(self.sync_root(), acquire_lock) {}

    private:
        inline explicit safe_unlockable_guard(not_null<core::sync_root> root,
                                              bool acquire_lock = true)
            : unlockable_sync_root_guard(root, acquire_lock) {}
    };

protected:
    inline bool is_synchronized() const { return sync_root()->is_synchronized(); }

private:
    auto_sync_root_holder const sync_root_;
};

}  // export namespace wxl::core
