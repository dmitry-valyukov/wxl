module;

#include "abi.h"

export module wxl.core:lock_guard;

import :noncopyable;

export namespace wxl::core {

template <typename S>
concept lockable_t = requires(S& s) {
    s.acquire();
    s.release();
};

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

inline constexpr already_locked_t already_locked{};

/// Can guard any sync primitive with acquire/release methods.
template <lockable_t S>
class lock_guard : public noncopyable
{
public:
    explicit lock_guard(S& lock) : lock_(lock) { lock.acquire(); }

    lock_guard(S& lock, already_locked_t) : lock_(lock) {}

    // TODO: убрать!!!ё
    lock_guard(S& lock, bool lock_now) : lock_(lock) {
        if (lock_now) lock_.acquire();
    }

    ~lock_guard() { lock_.release(); }

private:
    S& lock_;
};

template <try_lockable_t S>
class unlockable_guard : public noncopyable
{
public:
    explicit unlockable_guard(S& m, bool acquire_lock = true) : m_(m), locked_by_me_(acquire_lock) {
        if (acquire_lock) m.acquire();
    }

    explicit unlockable_guard(S& m, already_locked_t) : unlockable_guard(m, false) {}

    ~unlockable_guard() {
        if (locked_by_me_) [[likely]]
            m_.release();
    }

    void lock() {
        assert(!locked_by_me_);
        locked_by_me_ = true;
        m_.acquire();
    }

    bool try_lock() {
        assert(!locked_by_me_);
        bool locked = m_.try_acquire();
        locked_by_me_ = locked;
        return locked;
    }

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
