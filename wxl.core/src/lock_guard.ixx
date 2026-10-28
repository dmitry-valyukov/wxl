module;

#include "abi.h"

export module wxl.core:lock_guard;

import :noncopyable;

export namespace wxl::core {

/// Specifies that \p S provides `acquire` and `release` members.
template <typename S>
concept lockable_t = requires(S& s) {
    s.acquire();
    s.release();
};

/// Specifies that \p S provides `acquire`, `release` and `try_acquire` members.
template <typename S>
concept try_lockable_t = requires(S& s) {
    s.acquire();
    s.release();
    s.try_acquire();
};

/// Tag: the caller has already acquired the lock, the guard only has to release it.
struct already_locked_t {
    explicit already_locked_t() = default;
};

/// The `wxl::core::already_locked_t` value that selects the guard constructors for a lock the
/// caller already holds.
inline constexpr already_locked_t already_locked{};

/// Provides a scoped guard for any type that satisfies `wxl::core::lockable_t`: the lock is
/// released when the guard is destroyed.
template <lockable_t S>
class lock_guard : public noncopyable
{
public:
    /// Constructs the guard and acquires \p lock.
    explicit lock_guard(S& lock) : lock_(lock) { lock.acquire(); }

    /// Constructs the guard over \p lock, which the caller already holds, without acquiring
    /// it.
    lock_guard(S& lock, already_locked_t) : lock_(lock) {}

    // TODO: убрать!!!ё
    /// Constructs the guard and acquires \p lock if \p lock_now is `true`. The destructor
    /// releases \p lock in either case, so with `false` the caller must already hold it.
    lock_guard(S& lock, bool lock_now) : lock_(lock) {
        if (lock_now) lock_.acquire();
    }

    ~lock_guard() { lock_.release(); }

private:
    S& lock_;
};

/// Provides a scoped guard for a type that satisfies `wxl::core::try_lockable_t` that can
/// release and reacquire the lock during its lifetime. The destructor releases the lock only
/// if the guard holds it.
template <try_lockable_t S>
class unlockable_guard : public noncopyable
{
public:
    /// Constructs the guard and acquires \p m if \p acquire_lock is `true`; otherwise the
    /// guard starts without holding the lock.
    explicit unlockable_guard(S& m, bool acquire_lock = true) : m_(m), locked_by_me_(acquire_lock) {
        if (acquire_lock) m.acquire();
    }

    /// Constructs the guard over \p m without acquiring it. As with `acquire_lock` set to
    /// `false`, the guard starts without holding the lock, and the destructor does not
    /// release \p m.
    explicit unlockable_guard(S& m, already_locked_t) : unlockable_guard(m, false) {}

    ~unlockable_guard() {
        if (locked_by_me_) [[likely]]
            m_.release();
    }

    /// Acquires the lock, blocking until it is available. The guard must not hold the lock.
    void lock() {
        assert(!locked_by_me_);
        locked_by_me_ = true;
        m_.acquire();
    }

    /// Attempts to acquire the lock through its `try_acquire` member. The guard must not hold
    /// the lock.
    /// \return `true` if the lock was acquired.
    bool try_lock() {
        assert(!locked_by_me_);
        bool locked = m_.try_acquire();
        locked_by_me_ = locked;
        return locked;
    }

    /// Releases the lock. The guard must hold it.
    void unlock() {
        assert(locked_by_me_);
        locked_by_me_ = false;
        m_.release();
    }

private:
    S& m_;
    bool locked_by_me_;
};

}  // namespace wxl::core
