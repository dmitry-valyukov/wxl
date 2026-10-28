module;

#include "abi.h"
#include <intrin.h>

export module wxl.core:spin_lock;

import :noncopyable;
import std;

export namespace wxl::core {

/// Hints to the processor that this is a spin-wait iteration: on x86 it slows the loop to
/// the memory-order-violation cost and frees the pipeline for the sibling hyper-thread, on
/// ARM64 it yields the same way. Not a wait -- only ever for a retry whose contention is
/// resolved in a handful of instructions by another core that is already running.
inline void cpu_pause() noexcept {
#if defined(_M_ARM64)
    __yield();
#else
    _mm_pause();
#endif
}

/// Provides a scoped guard, like the guards for mutexes, over a lock word held in a
/// `std::atomic<ssize_t>`: `0` means free and `1` means held. Acquisition spins with
/// `wxl::core::cpu_pause` between attempts and never waits in the kernel.
class spin_lock : public noncopyable
{
public:
    /// Constructs the guard over \p target and, if \p acquire_lock is `true`, spins until the
    /// lock is acquired.
    inline explicit spin_lock(std::atomic<ssize_t>& target, bool acquire_lock = true)
        : target_(target), locked_by_me_(acquire_lock) {
        if (acquire_lock) lock_impl();
    }

    inline ~spin_lock() {
        if (locked_by_me_) {
            assert(target_.load(std::memory_order_relaxed) == 1);
            unlock_impl();
        }
    }

    /// Spins until the lock is acquired. The guard must not hold the lock.
    inline void lock() {
        assert(!locked_by_me_);
        lock_impl();
        locked_by_me_ = true;
    }

    /// Attempts to acquire the lock up to \p spin_count times, with `wxl::core::cpu_pause`
    /// between attempts. The guard must not hold the lock.
    /// \return `true` if the lock was acquired.
    inline bool try_lock(size_t spin_count = 128) {
        assert(!locked_by_me_);
        return (locked_by_me_ = try_lock_impl(spin_count));
    }

    /// Releases the lock. The guard must hold it.
    inline void unlock() {
        assert(locked_by_me_);
        unlock_impl();
        locked_by_me_ = false;
    }

    /// Tests whether this guard holds the lock.
    inline bool locked() const { return locked_by_me_; }

private:
    void lock_impl() noexcept;
    bool try_lock_impl(size_t spin_count) noexcept;
    void unlock_impl() noexcept;

private:
    std::atomic<ssize_t>& target_;
    bool locked_by_me_;
};

}  // export namespace wxl::core
